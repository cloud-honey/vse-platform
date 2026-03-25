#include "domain/TransportSystem.h"
#include <spdlog/spdlog.h>
#include <algorithm>
#include <cmath>
#include <climits>

namespace vse {

TransportSystem::TransportSystem(IGridSystem& grid, EventBus& bus,
                                  const ConfigManager& config)
    : grid_(grid)
    , eventBus_(bus)
    , doorOpenTicks_(config.getInt("elevator.doorOpenTicks", 3))
    , speedTilesPerTick_(config.getInt("elevator.speedTilesPerTick", 1))
    , defaultCapacity_(config.getInt("elevator.capacity", 8))
{}

Result<EntityId> TransportSystem::createElevator(int shaftX, int bottomFloor,
                                                   int topFloor, int capacity)
{
    if (shaftX < 0 || shaftX >= grid_.floorWidth()) {
        return Result<EntityId>::failure(ErrorCode::InvalidArgument);
    }
    if (bottomFloor < 0 || topFloor < bottomFloor ||
        topFloor >= grid_.maxFloors()) {
        return Result<EntityId>::failure(ErrorCode::InvalidArgument);
    }
    if (capacity <= 0) {
        return Result<EntityId>::failure(ErrorCode::InvalidArgument);
    }

    // shaftX, bottomFloor 조합 중복 체크
    for (auto& [id, car] : cars_) {
        if (car.shaftX == shaftX && car.bottomFloor == bottomFloor &&
            car.topFloor == topFloor) {
            spdlog::warn("TransportSystem::createElevator: duplicate elevator at shaftX={}", shaftX);
            return Result<EntityId>::failure(ErrorCode::InvalidArgument);
        }
    }

    // EntityId를 entt::registry에서 발급 (ECS 일관성 유지)
    entt::entity entity = registry_.create();
    EntityId id = static_cast<EntityId>(entity);
    ElevatorCar car;
    car.id           = id;
    car.shaftX       = shaftX;
    car.bottomFloor  = bottomFloor;
    car.topFloor     = topFloor;
    car.capacity     = capacity;
    car.currentFloor = bottomFloor;
    car.floatFloor   = static_cast<float>(bottomFloor);
    car.state        = ElevatorState::Idle;
    car.direction    = ElevatorDirection::Idle;

    cars_[static_cast<uint32_t>(id)] = std::move(car);

    spdlog::debug("TransportSystem::createElevator: id={}, shaftX={}, {}~{}",
                  static_cast<uint32_t>(id), shaftX, bottomFloor, topFloor);

    // 생성 시 ElevatorArrived 미발행 — "생성"과 "도착" 이벤트는 의미가 다름
    return Result<EntityId>::success(id);
}

void TransportSystem::callElevator(int shaftX, int floor,
                                    ElevatorDirection preferredDir)
{
    // 같은 shaftX의 car 중 최적 car 선택 (다중 car 배차)
    uint32_t bestKey = pickBestCar(shaftX, floor, preferredDir);
    if (bestKey == UINT32_MAX) {
        spdlog::warn("TransportSystem::callElevator: no elevator for shaftX={}", shaftX);
        return;
    }

    ElevatorCar& car = cars_[bestKey];
    HallCall hc{floor, preferredDir};
    car.hallCalls.insert(hc);  // 중복 자동 병합 (set)

    // 대기 인원 증가
    waitingCount_[floor]++;

    spdlog::debug("TransportSystem::callElevator: shaftX={}, floor={}, carId={}",
                  shaftX, floor, static_cast<uint32_t>(car.id));
}

uint32_t TransportSystem::pickBestCar(int shaftX, int floor,
                                       ElevatorDirection dir) const
{
    // 같은 shaftX, 운행 범위에 floor가 포함된 car들 수집 후 최적 선택
    // 우선순위:
    //   1순위: Idle car 중 가장 가까운 것
    //   2순위: 현재 방향이 요청 방향과 일치하고, 이동 중 floor가 경로 상에 있는 것
    //   3순위: 그 외 가장 가까운 것
    uint32_t bestKey  = UINT32_MAX;
    int      bestScore = INT_MAX;  // 낮을수록 좋음

    for (auto& [uid, car] : cars_) {
        if (car.shaftX != shaftX) continue;
        if (floor < car.bottomFloor || floor > car.topFloor) continue;

        int dist = std::abs(car.currentFloor - floor);
        int score;

        if (car.state == ElevatorState::Idle) {
            score = dist;  // 1순위: Idle + 거리
        } else if ((car.direction == ElevatorDirection::Up && dir == ElevatorDirection::Up
                    && car.currentFloor <= floor) ||
                   (car.direction == ElevatorDirection::Down && dir == ElevatorDirection::Down
                    && car.currentFloor >= floor)) {
            score = 1000 + dist;  // 2순위: 같은 방향 경로상
        } else {
            score = 2000 + dist;  // 3순위: 반대 방향
        }

        if (score < bestScore) {
            bestScore = score;
            bestKey   = uid;
        }
    }
    return bestKey;
}

Result<bool> TransportSystem::boardPassenger(EntityId elevator,
                                               EntityId agent, int targetFloor)
{
    auto it = cars_.find(static_cast<uint32_t>(elevator));
    if (it == cars_.end()) {
        return Result<bool>::failure(ErrorCode::InvalidArgument);
    }

    ElevatorCar& car = it->second;

    // Boarding 상태가 아니면 탑승 불가
    if (car.state != ElevatorState::Boarding) {
        return Result<bool>::failure(ErrorCode::InvalidArgument);
    }

    // 정원 초과
    if (static_cast<int>(car.passengers.size()) >= car.capacity) {
        return Result<bool>::failure(ErrorCode::InvalidArgument);
    }

    // 목적지 층 범위 확인
    if (targetFloor < car.bottomFloor || targetFloor > car.topFloor) {
        return Result<bool>::failure(ErrorCode::OutOfRange);
    }

    car.passengers.push_back(agent);
    car.carCalls.insert(targetFloor);

    // 대기 인원 감소
    if (waitingCount_.count(car.currentFloor) && waitingCount_[car.currentFloor] > 0) {
        waitingCount_[car.currentFloor]--;
    }

    spdlog::debug("TransportSystem::boardPassenger: elevator={}, agent={}, target={}",
                  static_cast<uint32_t>(elevator),
                  static_cast<uint32_t>(agent),
                  targetFloor);

    return Result<bool>::success(true);
}

void TransportSystem::exitPassenger(EntityId elevator, EntityId agent)
{
    auto it = cars_.find(static_cast<uint32_t>(elevator));
    if (it == cars_.end()) return;

    ElevatorCar& car = it->second;
    auto& p = car.passengers;
    p.erase(std::remove(p.begin(), p.end(), agent), p.end());

    spdlog::debug("TransportSystem::exitPassenger: elevator={}, agent={}",
                  static_cast<uint32_t>(elevator), static_cast<uint32_t>(agent));
}

void TransportSystem::update(const GameTime& time)
{
    for (auto& [uid, car] : cars_) {
        tickCar(car, time);
    }
}

void TransportSystem::tickCar(ElevatorCar& car, const GameTime& /*time*/)
{
    switch (car.state) {

    case ElevatorState::Idle: {
        // 같은 층 홀 콜이 있으면 바로 문 열기 (lookNextTarget은 currentFloor 제외)
        bool sameFloorCall = false;
        for (auto& hc : car.hallCalls) {
            if (hc.floor == car.currentFloor) { sameFloorCall = true; break; }
        }
        if (sameFloorCall) {
            car.state     = ElevatorState::DoorOpening;
            car.doorTicks = doorOpenTicks_;
            break;
        }

        int next = lookNextTarget(car);
        if (next == -1) break;  // 할 일 없음

        car.nextTarget = next;
        if (next > car.currentFloor) {
            car.state     = ElevatorState::MovingUp;
            car.direction = ElevatorDirection::Up;
        } else if (next < car.currentFloor) {
            car.state     = ElevatorState::MovingDown;
            car.direction = ElevatorDirection::Down;
        } else {
            // 이미 같은 층 — 바로 문 열기
            car.state    = ElevatorState::DoorOpening;
            car.doorTicks = doorOpenTicks_;
        }
        break;
    }

    case ElevatorState::MovingUp:
    case ElevatorState::MovingDown: {
        // 한 틱에 speedTilesPerTick 이동
        int delta = (car.state == ElevatorState::MovingUp) ? speedTilesPerTick_
                                                            : -speedTilesPerTick_;
        car.currentFloor = std::clamp(car.currentFloor + delta,
                                       car.bottomFloor, car.topFloor);
        car.floatFloor   = static_cast<float>(car.currentFloor);

        // 목적지 도착
        if (car.currentFloor == car.nextTarget) {
            car.state     = ElevatorState::DoorOpening;
            car.doorTicks = doorOpenTicks_;
            publishEvent(EventType::ElevatorArrived, car.id);
        }
        // 중간 홀 콜 처리 (같은 방향)
        else {
            for (auto& hc : car.hallCalls) {
                if (hc.floor == car.currentFloor) {
                    // 같은 방향이거나 Idle 호출이면 정차
                    if (hc.direction == ElevatorDirection::Idle ||
                        (car.direction == ElevatorDirection::Up && hc.direction == ElevatorDirection::Up) ||
                        (car.direction == ElevatorDirection::Down && hc.direction == ElevatorDirection::Down)) {
                        car.nextTarget = car.currentFloor;
                        car.state      = ElevatorState::DoorOpening;
                        car.doorTicks  = doorOpenTicks_;
                        publishEvent(EventType::ElevatorArrived, car.id);
                        break;
                    }
                }
            }
        }
        break;
    }

    case ElevatorState::DoorOpening: {
        // 1틱에 바로 Boarding으로 전환
        car.state = ElevatorState::Boarding;
        break;
    }

    case ElevatorState::Boarding: {
        // doorTicks 카운트다운
        if (car.doorTicks > 0) {
            --car.doorTicks;
        }
        if (car.doorTicks <= 0) {
            // 현재 층 홀 콜 제거
            for (auto it = car.hallCalls.begin(); it != car.hallCalls.end(); ) {
                if (it->floor == car.currentFloor) it = car.hallCalls.erase(it);
                else ++it;
            }
            // 현재 층 카 콜 제거
            car.carCalls.erase(car.currentFloor);

            car.state = ElevatorState::DoorClosing;
        }
        break;
    }

    case ElevatorState::DoorClosing: {
        // 같은 층에 새로운 홀 콜이 있으면 바로 재오픈
        // (lookNextTarget은 currentFloor를 제외하므로 별도 체크)
        bool sameFloorCall = false;
        for (auto& hc : car.hallCalls) {
            if (hc.floor == car.currentFloor) { sameFloorCall = true; break; }
        }
        if (sameFloorCall) {
            car.state     = ElevatorState::DoorOpening;
            car.doorTicks = doorOpenTicks_;
            break;
        }

        // 다음 목적지 결정
        int next = lookNextTarget(car);
        if (next == -1) {
            // 할 일 없음 → Idle
            car.state     = ElevatorState::Idle;
            car.direction = ElevatorDirection::Idle;
            car.nextTarget = -1;
        } else if (next == car.currentFloor) {
            // carCalls에 현재 층이 있는 경우 (lookNextTarget이 currentFloor 반환하는 케이스)
            car.state     = ElevatorState::DoorOpening;
            car.doorTicks = doorOpenTicks_;
        } else {
            car.nextTarget = next;
            car.state      = (next > car.currentFloor) ? ElevatorState::MovingUp
                                                        : ElevatorState::MovingDown;
            car.direction  = (next > car.currentFloor) ? ElevatorDirection::Up
                                                        : ElevatorDirection::Down;
        }
        break;
    }
    }
}

int TransportSystem::lookNextTarget(const ElevatorCar& car) const
{
    // 모든 콜 수집
    std::vector<int> allTargets;
    for (int f : car.carCalls)   allTargets.push_back(f);
    for (auto& hc : car.hallCalls) allTargets.push_back(hc.floor);

    if (allTargets.empty()) return -1;

    // LOOK 알고리즘:
    // 현재 방향으로 같은 방향 목적지 우선 처리
    // 없으면 반대 방향 중 가장 가까운 것
    std::vector<int> sameDir, opposite;

    for (int f : allTargets) {
        if (car.direction == ElevatorDirection::Up && f > car.currentFloor)
            sameDir.push_back(f);
        else if (car.direction == ElevatorDirection::Down && f < car.currentFloor)
            sameDir.push_back(f);
        else if (f != car.currentFloor)
            opposite.push_back(f);
    }

    if (!sameDir.empty()) {
        // 같은 방향: Up이면 가장 낮은 것, Down이면 가장 높은 것
        if (car.direction == ElevatorDirection::Up)
            return *std::min_element(sameDir.begin(), sameDir.end());
        else
            return *std::max_element(sameDir.begin(), sameDir.end());
    }

    if (!opposite.empty()) {
        // 반대 방향: 현재 위치에서 가장 가까운 것
        return *std::min_element(opposite.begin(), opposite.end(),
            [&](int a, int b) {
                return std::abs(a - car.currentFloor) < std::abs(b - car.currentFloor);
            });
    }

    return -1;
}

std::optional<ElevatorSnapshot> TransportSystem::getElevatorState(EntityId id) const
{
    auto it = cars_.find(static_cast<uint32_t>(id));
    if (it == cars_.end()) return std::nullopt;

    const ElevatorCar& car = it->second;
    ElevatorSnapshot snap;
    snap.id                 = car.id;
    snap.shaftX             = car.shaftX;
    snap.currentFloor       = car.currentFloor;
    snap.interpolatedFloor  = car.floatFloor;
    snap.state              = car.state;
    snap.direction          = car.direction;
    snap.passengerCount     = static_cast<int>(car.passengers.size());
    snap.capacity           = car.capacity;
    snap.passengers         = car.passengers;
    for (int f : car.carCalls)   snap.callQueue.push_back(f);
    for (auto& hc : car.hallCalls) snap.callQueue.push_back(hc.floor);

    return snap;
}

std::vector<EntityId> TransportSystem::getElevatorsAtFloor(int floor) const
{
    std::vector<EntityId> result;
    for (auto& [uid, car] : cars_) {
        if (car.currentFloor == floor) result.push_back(car.id);
    }
    return result;
}

int TransportSystem::getWaitingCount(int floor) const
{
    auto it = waitingCount_.find(floor);
    return (it != waitingCount_.end()) ? it->second : 0;
}

std::vector<EntityId> TransportSystem::getAllElevators() const
{
    std::vector<EntityId> result;
    result.reserve(cars_.size());
    for (auto& [uid, car] : cars_) result.push_back(car.id);
    return result;
}

void TransportSystem::publishEvent(EventType type, EntityId source)
{
    Event ev;
    ev.type   = type;
    ev.source = source;
    eventBus_.publish(ev);
}

// ── SaveLoad export/import ───────────────────────────────────────────────────

nlohmann::json TransportSystem::exportState() const {
    using json = nlohmann::json;
    json j;
    json carsArr = json::array();
    for (const auto& [key, car] : cars_) {
        json cj;
        cj["id"]           = static_cast<uint32_t>(car.id);
        cj["shaftX"]       = car.shaftX;
        cj["bottomFloor"]  = car.bottomFloor;
        cj["topFloor"]     = car.topFloor;
        cj["capacity"]     = car.capacity;
        cj["currentFloor"] = car.currentFloor;
        cj["floatFloor"]   = car.floatFloor;
        cj["state"]        = static_cast<int>(car.state);
        cj["direction"]    = static_cast<int>(car.direction);
        cj["doorTicks"]    = car.doorTicks;
        cj["nextTarget"]   = car.nextTarget;

        json passArr = json::array();
        for (auto p : car.passengers) {
            passArr.push_back(static_cast<uint32_t>(p));
        }
        cj["passengers"] = passArr;

        json carCallsArr = json::array();
        for (int f : car.carCalls) {
            carCallsArr.push_back(f);
        }
        cj["carCalls"] = carCallsArr;

        json hallCallsArr = json::array();
        for (const auto& hc : car.hallCalls) {
            hallCallsArr.push_back({
                {"floor", hc.floor},
                {"direction", static_cast<int>(hc.direction)}
            });
        }
        cj["hallCalls"] = hallCallsArr;

        carsArr.push_back(cj);
    }
    j["cars"] = carsArr;

    json waitArr = json::object();
    for (const auto& [floor, count] : waitingCount_) {
        waitArr[std::to_string(floor)] = count;
    }
    j["waitingCount"] = waitArr;

    return j;
}

void TransportSystem::importState(const nlohmann::json& j) {
    cars_.clear();
    waitingCount_.clear();

    if (j.contains("cars")) {
        for (const auto& cj : j["cars"]) {
            ElevatorCar car;
            // Create entity in internal registry to get a valid EntityId
            car.id           = registry_.create();
            car.shaftX       = cj.value("shaftX", 0);
            car.bottomFloor  = cj.value("bottomFloor", 0);
            car.topFloor     = cj.value("topFloor", 0);
            car.capacity     = cj.value("capacity", 8);
            car.currentFloor = cj.value("currentFloor", 0);
            car.floatFloor   = cj.value("floatFloor", 0.0f);
            car.state        = static_cast<ElevatorState>(cj.value("state", 0));
            car.direction    = static_cast<ElevatorDirection>(cj.value("direction", 0));
            car.doorTicks    = cj.value("doorTicks", 0);
            car.nextTarget   = cj.value("nextTarget", -1);

            if (cj.contains("passengers")) {
                for (const auto& p : cj["passengers"]) {
                    car.passengers.push_back(static_cast<EntityId>(p.get<uint32_t>()));
                }
            }
            if (cj.contains("carCalls")) {
                for (const auto& f : cj["carCalls"]) {
                    car.carCalls.insert(f.get<int>());
                }
            }
            if (cj.contains("hallCalls")) {
                for (const auto& hc : cj["hallCalls"]) {
                    HallCall call;
                    call.floor     = hc.value("floor", 0);
                    call.direction = static_cast<ElevatorDirection>(hc.value("direction", 0));
                    car.hallCalls.insert(call);
                }
            }

            cars_[static_cast<uint32_t>(car.id)] = car;
        }
    }

    if (j.contains("waitingCount")) {
        for (auto& [key, val] : j["waitingCount"].items()) {
            waitingCount_[std::stoi(key)] = val.get<int>();
        }
    }

    spdlog::debug("TransportSystem::importState: restored {} elevator cars", cars_.size());
}

} // namespace vse
