# VSE Platform — TASK-01-001 & TASK-01-002 작업 결과 보고서

> 작성일: 2026-03-24  
> 검토 대상: Sprint 1 — Layer 0 Core Runtime 1~2번 태스크  
> 목적: 외부 AI 검증·검토용

---

## TASK-01-001: CMake 프로젝트 구조

### 작업 내용

| 항목 | 내용 |
|------|------|
| 목표 | C++17 + SDL2 + Catch2 기반 빌드 환경 구축 |
| 플랫폼 | Linux (개발), macOS arm64 (CI) |
| 완료일 | 2026-03-24 |
| 커밋 | a179345, c204e67 |

### 디렉토리 구조

```
vse-platform/
├── CMakeLists.txt                  # 루트 빌드
├── cmake/
│   └── FetchDependencies.cmake     # 외부 의존성 (spdlog, Catch2, imgui, EnTT)
├── include/
│   ├── core/                       # Layer 0 Core API (인터페이스·타입)
│   ├── domain/                     # Layer 1 (게임 규칙)
│   ├── content/                    # Layer 2 (콘텐츠 로더)
│   └── renderer/                   # Layer 3 (SDL2)
├── src/
│   ├── core/                       # Layer 0 Core Runtime 구현
│   ├── domain/
│   ├── content/
│   └── renderer/
├── tests/                          # Catch2 테스트
└── assets/
    └── config/
        └── game_config.json
```

### 주요 의존성

| 라이브러리 | 버전 | 설치 방법 |
|-----------|------|-----------|
| SDL2 | 시스템 패키지 | `find_package(SDL2)` |
| SDL2_image | 시스템 패키지 | `find_package` |
| SDL2_ttf | 시스템 패키지 | `find_package` |
| spdlog | 1.14.1 | FetchContent |
| Catch2 | 3.x | FetchContent |
| EnTT | 3.13.x | FetchContent |
| Dear ImGui | latest | FetchContent |

### 빌드 검증 결과

```
cmake --build build → 성공
ctest --output-on-failure → 1/1 통과 (placeholder 테스트)
```

### 특이사항

- SDL2를 `FetchContent`로 소스 빌드하면 CI 시간이 길어져 `find_package`로 전환
- GitHub Actions macOS 러너가 arm64(M1)이므로 `CMAKE_OSX_ARCHITECTURES=arm64` 단일 설정
- Linux 개발 환경: `apt install libsdl2-dev libsdl2-image-dev libsdl2-ttf-dev`

---

## TASK-01-002: SimClock 구현 (100ms tick, 렌더링 분리)

### 작업 내용

| 항목 | 내용 |
|------|------|
| 목표 | SimClock + EventBus 구현, 렌더링 루프와 완전 분리 |
| 레이어 | Layer 0 Core Runtime |
| 완료일 | 2026-03-24 |

### 구현 파일

| 파일 | 역할 |
|------|------|
| `include/core/SimClock.h` | SimClock 클래스 선언 |
| `src/core/SimClock.cpp` | SimClock 구현 (tick 진행, 시간 경계 이벤트) |
| `include/core/EventBus.h` | EventBus 클래스 선언 |
| `src/core/EventBus.cpp` | EventBus 구현 (deferred 큐, double-buffer) |
| `tests/test_SimClock.cpp` | Catch2 테스트 12개 |

---

### SimClock 구현 상세

#### 핵심 규칙

- **1 tick = 1 game minute** (Phase 1 하드코딩)
- **1440 ticks = 1 game day** (24h × 60min)
- 실제 시간 100ms = 1 sim tick (Bootstrapper의 accumulator가 관리)
- `advanceTick()` — paused 시 아무것도 하지 않음
- `setSpeed(n)` — 1x/2x/4x만 허용, assert로 강제

#### 시간 변환 (GameTime::fromTick)

```cpp
// Types.h에 정의된 변환 공식
static GameTime fromTick(SimTick tick) {
    int totalMin = static_cast<int>(tick);
    return GameTime{
        (totalMin % 1440) / 60,   // hour (0~23)
        totalMin % 60,             // minute (0~59)
        totalMin / 1440            // day (0~)
    };
}
```

예시: tick=90 → hour=1, minute=30, day=0

#### 이벤트 발행 조건

| 이벤트 | 발행 조건 |
|--------|-----------|
| `TickAdvanced` | 매 `advanceTick()` 호출 시 |
| `HourChanged` | hour가 바뀔 때 (60 tick마다) |
| `DayChanged` | day가 바뀔 때 (1440 tick마다) |

모든 이벤트는 `EventBus.publish()`를 통해 **deferred 큐에 추가** — 즉시 전달 없음.

---

### EventBus 구현 상세

#### 핵심 설계

- **deferred-only**: `publish()` → `queue_` 추가만, 즉시 전달 금지
- **double-buffer**: `flush()` 시 `queue_` ↔ `processing_` 스왑
  - 핸들러 내부에서 `publish()` 호출해도 현재 flush에 영향 없음 (무한루프 방지)
- **subscribe/unsubscribe**: `SubscriptionId` 반환, ID로 구독 해제

#### flush() 동작 순서

```
1. swap(queue_, processing_)  // queue_ 비워짐
2. for each event in processing_:
     → 구독자 목록 복사 (unsubscribe 안전)
     → callback(event) 호출
3. processing_.clear()
```

---

### 테스트 결과

```
Test project /home/sykim/.openclaw/workspace/vse-platform/build
 1/12  placeholder — infrastructure sanity  PASSED
 2/12  SimClock 초기 상태                   PASSED
 3/12  SimClock advanceTick()               PASSED
 4/12  SimClock pause/resume               PASSED
 5/12  SimClock 속도 배수                   PASSED
 6/12  SimClock HourChanged 이벤트         PASSED
 7/12  SimClock DayChanged 이벤트          PASSED
 8/12  SimClock GameTime 변환              PASSED
 9/12  SimClock restoreState              PASSED
10/12  EventBus deferred publish          PASSED
11/12  EventBus unsubscribe              PASSED
12/12  EventBus pendingCount             PASSED

100% tests passed, 0 tests failed out of 12
Total Test time: 0.03 sec
```

---

### 설계 원칙 준수 여부

| 원칙 | 준수 여부 | 비고 |
|------|-----------|------|
| Layer 경계 — SimClock에 게임 로직 없음 | ✅ | 만족도·임대료 등 없음 |
| Layer 경계 — SDL2 의존성 없음 | ✅ | Core Runtime은 SDL2 미참조 |
| exception 사용 금지 | ✅ | assert + return 방식 |
| EventBus deferred-only | ✅ | publish → 즉시 전달 없음 |
| double-buffer flush | ✅ | queue_ ↔ processing_ 스왑 |
| SimTick = uint64_t | ✅ | Types.h 공유 타입 사용 |

---

### 미결 사항 / 검토 요청 항목

1. **speed multiplier 처리 방식**: 현재 `advanceTick()`에서 `tick_ += speed_`로 처리. 스펙은 "Bootstrapper가 accumulator를 speed 배로 더 많이 호출"로 정의되어 있어 이 구현이 스펙 의도와 다를 수 있음. 검토 필요.

2. **HourChanged/DayChanged 경계 판정**: `checkTimeBoundaries(oldTick, newTick)`에서 `oldHour != newHour`로 판정. speed=4일 때 한 번의 advanceTick으로 여러 경계를 건너뛸 경우 이벤트가 1개만 발행됨. 허용 여부 확인 필요.

3. **restoreState에서 이벤트 발행**: 현재 `restoreState()`도 `checkTimeBoundaries()`를 호출해 경계 이벤트를 발행함. 세이브 로드 시 불필요한 이벤트 발행 가능성 있음. 의도적인 설계인지 확인 필요.

---

## 전체 Sprint 1 진행 현황

| Task | 제목 | 상태 |
|------|------|------|
| TASK-01-001 | CMake 프로젝트 구조 | ✅ 완료 |
| TASK-01-002 | SimClock 구현 | ✅ 완료 |
| TASK-01-003 | ConfigManager + JSON 로딩 | ⏳ 대기 |
| TASK-01-004 | EventBus 기반 도메인 연결 | ⏳ 대기 |
| TASK-01-005 | GridSystem 기반 구현 | ⏳ 대기 |
| TASK-01-006 | AgentSystem 기초 | ⏳ 대기 |
| TASK-01-007 | EconomyEngine 기초 | ⏳ 대기 |
| TASK-01-008 | SDL2 렌더러 초기화 | ⏳ 대기 |
| TASK-01-009 | 타일 렌더링 | ⏳ 대기 |
| TASK-01-010 | 입력 처리 | ⏳ 대기 |

---

*이 보고서는 외부 AI 검증·검토 요청용입니다. 검토 시 "미결 사항" 항목 위주로 의견 주시면 됩니다.*
