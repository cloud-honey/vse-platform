# VSE Platform — TASK-01-003 작업 결과 보고서

> 작성일: 2026-03-24  
> 검토 대상: Sprint 1 — TASK-01-003 ConfigManager  
> 목적: 외부 AI 검증·검토용

---

## 1. 작업 개요

| 항목 | 내용 |
|------|------|
| 태스크 | TASK-01-003: ConfigManager + JSON 로딩 |
| 레이어 | Layer 0 Core Runtime |
| 완료일 | 2026-03-24 |
| 커밋 | c9bf3a6 |
| 테스트 | 26/26 통과 (SimClock 14 + ConfigManager 12) |

---

## 2. 구현 파일

| 파일 | 역할 |
|------|------|
| `include/core/ConfigManager.h` | ConfigManager 클래스 선언 |
| `src/core/ConfigManager.cpp` | ConfigManager 구현 |
| `tests/test_ConfigManager.cpp` | Catch2 테스트 12개 |

---

## 3. ConfigManager 구현 상세

### 3.1 인터페이스 (헤더)

```cpp
class ConfigManager {
public:
    Result<bool> loadFromFile(const std::string& path);

    int         getInt   (const std::string& key, int defaultVal = 0) const;
    float       getFloat (const std::string& key, float defaultVal = 0.0f) const;
    std::string getString(const std::string& key, const std::string& defaultVal = "") const;
    bool        getBool  (const std::string& key, bool defaultVal = false) const;

    const nlohmann::json& raw() const;
    bool has(const std::string& key) const;

private:
    nlohmann::json data_;
    const nlohmann::json* find(const std::string& key) const;  // dot notation 탐색
};
```

### 3.2 핵심 구현 — dot notation 탐색 (`find()`)

```cpp
const nlohmann::json* ConfigManager::find(const std::string& key) const {
    const nlohmann::json* current = &data_;
    size_t start = 0;
    size_t end = key.find('.');

    while (end != std::string::npos) {
        std::string part = key.substr(start, end - start);
        if (!current->contains(part)) return nullptr;
        current = &(*current)[part];
        start = end + 1;
        end = key.find('.', start);
    }

    std::string lastPart = key.substr(start);
    if (!current->contains(lastPart)) return nullptr;
    return &(*current)[lastPart];
}
```

- `.` 구분자로 중첩 JSON 탐색
- 키 없으면 `nullptr` 반환 (exception 없음)
- 모든 getter는 `find()` 결과로 동작

### 3.3 `loadFromFile()` 구현

```cpp
Result<bool> ConfigManager::loadFromFile(const std::string& path) {
    std::ifstream file(path);
    if (!file.is_open())
        return Result<bool>::failure(ErrorCode::FileNotFound);

    std::stringstream buffer;
    buffer << file.rdbuf();
    file.close();

    auto result = nlohmann::json::parse(buffer.str(), nullptr, false);  // throw=false
    if (result.is_discarded())
        return Result<bool>::failure(ErrorCode::JsonParseError);

    data_ = result;
    return Result<bool>::success(true);
}
```

- `nlohmann::json::parse(..., false)` — exception 없이 실패 감지
- 파일 없음: `ErrorCode::FileNotFound`
- 파싱 실패: `ErrorCode::JsonParseError`

### 3.4 `getFloat()` 특이사항

```cpp
float ConfigManager::getFloat(const std::string& key, float defaultVal) const {
    const nlohmann::json* node = find(key);
    if (node && node->is_number_float())   return node->get<float>();
    if (node && node->is_number_integer()) return static_cast<float>(node->get<int>());
    return defaultVal;
}
```

- JSON 정수값(`32`, `100`)도 `getFloat()`로 가져올 수 있도록 처리
- `getInt()`는 정수만 허용 (float 키에 `getInt()` 호출 시 defaultVal 반환)

---

## 4. 테스트 결과

```
 1/26  placeholder — infrastructure sanity      PASSED
 2/26  SimClock 초기 상태                        PASSED
 3~14  (SimClock/EventBus 테스트 14개)           ALL PASSED
15/26  ConfigManager - 파일 로드 성공           PASSED
16/26  ConfigManager - 존재하지 않는 파일        PASSED
17/26  ConfigManager - getInt dot notation      PASSED
18/26  ConfigManager - getFloat                PASSED
19/26  ConfigManager - getString               PASSED
20/26  ConfigManager - getBool                 PASSED
21/26  ConfigManager - has()                   PASSED
22/26  ConfigManager - raw()                   PASSED
23/26  ConfigManager - default fallback        PASSED
24/26  ConfigManager - 잘못된 JSON 파일         PASSED
25/26  ConfigManager - 중첩 키 접근             PASSED
26/26  ConfigManager - 타입 변환               PASSED

100% tests passed, 0 tests failed out of 26
Total Test time: 0.06 sec
```

---

## 5. 트러블슈팅 기록

### 문제: 테스트에서 파일 경로 실패
- **원인**: `ctest`는 `build/` 디렉토리에서 실행되어 `assets/config/game_config.json` 상대경로 불일치
- **해결**: CMake에서 `VSE_PROJECT_ROOT` 매크로를 컴파일 타임에 주입
  ```cmake
  target_compile_definitions(TowerTycoonTests PRIVATE
      VSE_PROJECT_ROOT="${CMAKE_SOURCE_DIR}"
  )
  ```
  테스트에서 `std::string(VSE_PROJECT_ROOT) + "/assets/config/..."` 로 절대경로 사용

---

## 6. 설계 원칙 준수 여부

| 원칙 | 준수 여부 | 비고 |
|------|-----------|------|
| Layer 경계 — 게임 로직 없음 | ✅ | 만족도·임대료 계산 없음 |
| exception 사용 금지 | ✅ | `parse(..., false)` + `is_discarded()` |
| `Result<T>` 에러 처리 | ✅ | FileNotFound, JsonParseError |
| nlohmann::json 의존성 | ✅ | FetchContent로 이미 관리됨 |
| dot notation 지원 | ✅ | `find()` 헬퍼로 재귀 탐색 |

---

## 7. 미결 사항 / 검토 요청 항목

1. **`getInt()`의 float 키 처리**: 현재 `getInt("npc.movementSpeedTilesPerTick")`처럼 float 값에 getInt를 호출하면 defaultVal(0)을 반환. `static_cast<int>`로 변환해서 반환하는 게 나을지, 아니면 현재처럼 타입 불일치 시 default 반환이 맞는지 검토 필요.

2. **`find()` 성능**: 매 `get*()` 호출마다 dot notation 파싱 + 순회. 게임 루프 내에서 ConfigManager getter를 호출하지 않도록 설계해야 하는지, 아니면 캐싱이 필요한지 검토 필요. (현재 설계: 스타트업 시 로드, 이후 읽기 전용 — 성능 문제 없을 것으로 예상)

3. **빈 `data_` 상태에서의 동작**: `loadFromFile()` 호출 전에 getter를 호출하면 `data_`가 빈 JSON 객체(`{}`)이므로 모두 defaultVal 반환. 이 동작이 명시적으로 문서화되어야 하는지 검토 필요.

4. **`raw()` 변경 방어**: 현재 `raw()`가 `const nlohmann::json&` 반환이라 외부 수정은 불가. 충분한지 검토.

---

## 8. 참고: game_config.json 구조

```json
{
  "simulation": { "tickRateMs": 100, "maxNpcCount": 50, "maxFloors": 30, "timeScaleFactors": [1,2,4] },
  "grid":       { "tileSizePx": 32, "floorHeightPx": 32, "tilesPerFloor": 20 },
  "elevator":   { "maxShafts": 4, "speedTilesPerTick": 1, "capacity": 8 },
  "tenant": {
    "office":       { "baseRent": 500, "maintenanceCost": 100 },
    "residential":  { "baseRent": 300, "maintenanceCost": 80 },
    "commercial":   { "baseRent": 700, "maintenanceCost": 150 }
  },
  "npc":        { "movementSpeedTilesPerTick": 0.5, "satisfactionDecayRate": 0.01, "satisfactionGainRate": 0.05 },
  "rating":     { "thresholds": { "oneStar": 0.2, "twoStar": 0.4, "threeStar": 0.6, "fourStar": 0.8, "fiveStar": 1.0 } },
  "rendering":  { "targetFps": 60, "minimumFps": 30, "windowWidth": 1280, "windowHeight": 720 }
}
```

---

## 9. Sprint 1 전체 진행 현황

| Task | 제목 | 상태 |
|------|------|------|
| TASK-01-001 | CMake 프로젝트 구조 | ✅ 완료 |
| TASK-01-002 | SimClock 구현 | ✅ 완료 (검토 반영) |
| TASK-01-003 | ConfigManager + JSON 로딩 | ✅ 완료 |
| TASK-01-004 | EventBus 기반 도메인 연결 | ⏳ 대기 |
| TASK-01-005 | GridSystem 기반 구현 | ⏳ 대기 |
| TASK-01-006 | AgentSystem 기초 | ⏳ 대기 |
| TASK-01-007 | EconomyEngine 기초 | ⏳ 대기 |
| TASK-01-008 | SDL2 렌더러 초기화 | ⏳ 대기 |
| TASK-01-009 | 타일 렌더링 | ⏳ 대기 |
| TASK-01-010 | 입력 처리 | ⏳ 대기 |

---

*이 보고서는 외부 AI 검증·검토 요청용입니다. 7번 미결 사항 위주로 의견 주시면 됩니다.*
