# CLAUDE.md — VSE Platform Phase 1 구현 사양서
> Version: 1.0 | 대상: 붐(PM) + DeepSeek(개발) + Claude Code(렌더링)
> 이 파일이 모든 구현 판단의 최우선 기준이다. 여기 없는 것은 Human에게 먼저 묻는다.

---

## 프로젝트 개요

**Tower Tycoon** — SimTower 스타일 빌딩 관리 시뮬레이션
- 엔진: C++17 + SDL2 + Dear ImGui + EnTT
- 빌드: CMake
- 배포: macOS + Steam (Phase 1 한정)
- 목표: Steam 출시 가능한 MVP

---

## Phase 1 절대 범위 (이것만 만든다)

### 포함
- 타일 그리드 (30층 이하)
- NPC AI (시간대별 이동 패턴, 최대 50명)
- 엘리베이터 시뮬 (1축 2~4대, LOOK 알고리즘)
- 테넌트 3종 (사무실 / 주거 / 상업)
- 기본 경제 (수입 / 지출)
- 별점 시스템 (단순화)
- SDL2 렌더링 + Dear ImGui 디버그 툴
- macOS + Steam 빌드

### 제외 (코드 한 줄도 없어야 함)
- BAS / BACnet / IIoTAdapter
- 멀티플레이어 / INetworkAdapter 구현
- 재난 시스템
- iOS / Android 빌드
- VariableEncryption
- AuditLogger (spdlog로 대체)
- Destination Dispatch (v1.1)
- 부채 / 이자 / 인플레이션

### 설계만 있고 구현 없는 것 (슬롯 확보용)
- INetworkAdapter + IAuthority (인터페이스 선언, 메서드 비워둠)
- SpatialPartitioning (인터페이스 선언만, 구현은 성능 실측 후)

---

## 아키텍처 레이어

```
Layer 0  VSE Core       인터페이스만. 구체 구현 금지.
Layer 1  Domain Module  RealEstateDomain. 게임 로직 전담.
Layer 2  Content Pack   JSON + 스프라이트. 코드 변경 없이 DLC 가능.
Layer 3  SDL2 Renderer  IRenderCommand 기반. Dirty Rect.
```

**레이어 경계 위반 예시 (금지):**
- Layer 3 렌더러 내부에서 게임 로직 계산 → 금지
- Layer 0 인터페이스에 구체 구현 포함 → 금지
- Layer 1 도메인에서 SDL2 직접 호출 → 금지

---

## Layer 0 모듈 목록 (Phase 1 구현 기준)

| 모듈 | 구현 여부 | 비고 |
|---|---|---|
| IGridSystem | ✅ 구현 | |
| IAgentSystem | ✅ 구현 | |
| ITransportSystem | ✅ 구현 | |
| EventBus | ✅ 구현 | 단일 버스 + replicable 플래그. 듀얼 채널은 Phase 3. |
| IEconomyEngine | ✅ 구현 | 기본 수입/지출만 |
| SimClock | ✅ 구현 | 100ms 고정 틱. 렌더링과 분리. |
| LocaleManager | ✅ 구현 | ko / en 우선 |
| ContentRegistry | ✅ 구현 | 핫 리로드 포함 |
| ConfigManager | ✅ 구현 | |
| ISaveLoad | ✅ 구현 | MessagePack. 버전 번호 기록. Migration은 Phase 2. |
| Bootstrapper | ✅ 구현 | 초기화 순서 관리 |
| IAsyncLoader | ✅ 구현 | 리소스 비동기 로딩 |
| IMemoryPool | ✅ 구현 | NPC/타일 오브젝트 할당 최적화 |
| SpatialPartitioning | ⚠️ 선언만 | 성능 실측 후 구현 결정 |
| INetworkAdapter + IAuthority | ⚠️ 선언만 | 메서드 비워둠. Phase 3. |
| IIoTAdapter | ❌ 없음 | Phase 2. 슬롯 주석만 허용. |
| ISpatialRule | ❌ 없음 | Phase 2+. |
| AuditLogger | ❌ 없음 | spdlog 사용. |
| VariableEncryption | ❌ 없음 | Steam 출시 후 재검토. |

---

## 시뮬레이션 설정

| 항목 | 값 | 비고 |
|---|---|---|
| SimClock Tick | 100ms (10 TPS) | 고정. 변경 시 Human 승인 필요. |
| 렌더링 | 60fps 목표 | 시뮬과 독립 루프 |
| NPC 최대 | 50명 | Phase 1 하드 상한 |
| 층수 최대 | 30층 | Phase 1 하드 상한 |
| 엘리베이터 | 1축 2~4대 | LOOK 알고리즘 |
| 테넌트 종류 | 3종 | 사무실 / 주거 / 상업 |
| 타겟 프레임 | 60fps | 최소 30fps 유지 |
| 모바일 메모리 | 해당 없음 | Phase 1은 macOS만 |

---

## ECS 설정 (EnTT)

- 라이브러리: EnTT (Header-only, C++17)
- 커스텀 ECS 구현 금지
- 컴포넌트는 데이터만, 로직은 System에
- 엔티티 생명주기는 System 단위로 관리

```cpp
// 컴포넌트 예시 — 데이터만
struct PositionComponent { int x, y, floor; };
struct AgentComponent { AgentType type; float satisfaction; };

// System 예시 — 로직만
class AgentMovementSystem {
    void update(entt::registry& reg, float dt);
};
```

---

## EventBus 사용 규칙

```cpp
// Phase 1: 단일 EventBus + replicable 플래그
struct Event {
    EventType type;
    bool replicable = false;  // Phase 3 네트워크 동기화 대상 여부
    // payload...
};

// InternalEvent (시각 효과, 애니메이션 — replicable=false)
// ExportableEvent (상태 변경, 경제 이벤트 — replicable=true)
```

- 모든 이벤트는 타입 선언 시 replicable 여부 명시
- UI/애니메이션 이벤트는 replicable=false 기본
- 경제/상태 변경 이벤트는 replicable=true 기본

---

## 직렬화 (MessagePack)

- 세이브 파일 포맷: MessagePack
- 세이브 파일에 버전 번호 포함 (현재: v1)
- Migration 로직은 Phase 2에서 추가
- FlatBuffers 사용 금지 (Phase 1)

```cpp
// 버전 헤더 예시
struct SaveHeader {
    uint32_t version = 1;
    // ...
};
```

---

## 메모리 소유권 컨벤션

| 상황 | 사용 |
|---|---|
| 단일 소유자 명확 | `unique_ptr` |
| 공유 필요 (최소화) | `shared_ptr` |
| 소유권 없는 참조 | raw pointer 또는 reference |
| 컨테이너 내 폴리모피즘 | `unique_ptr<IBase>` |

- `new` / `delete` 직접 사용 금지
- EnTT registry가 관리하는 컴포넌트는 raw pointer 허용

---

## 에러 핸들링 정책 ✅ 확정

| 상황 | 방법 |
|---|---|
| 개발 중 불변 조건 위반 | `assert()` — 릴리즈 빌드에서 제거됨 |
| 런타임 복구 가능 오류 | `std::optional` 또는 error code 반환 |
| 런타임 복구 불가 오류 | spdlog로 기록 후 graceful shutdown |
| 예외(exception) | **사용 금지** |

```cpp
// CORRECT: error code return
std::optional<TileCoordinate> GridSystem::getTile(int x, int floor) {
    if (floor < 0 || floor >= MAX_FLOORS) return std::nullopt;
    return TileCoordinate{x, floor};
}

// WRONG: never do this
// throw std::out_of_range("floor out of bounds"); // FORBIDDEN
```

---

## 네이밍 컨벤션

| 대상 | 규칙 | 예시 |
|---|---|---|
| 인터페이스 | `I` 접두사 + PascalCase | `IGridSystem` |
| 클래스 | PascalCase | `ElevatorController` |
| 함수 | camelCase | `updatePosition()` |
| 변수 | camelCase | `currentFloor` |
| 상수 | UPPER_SNAKE | `MAX_NPC_COUNT` |
| 파일 | PascalCase | `GridSystem.cpp` |
| 컴포넌트 | `Component` 접미사 | `PositionComponent` |
| 시스템 | `System` 접미사 | `AgentMovementSystem` |

---

## 하드코딩 금지 목록

다음 수치는 반드시 외부 JSON / ConfigManager에서 읽어야 한다:

- NPC 이동 속도, 만족도 증감 수치
- 엘리베이터 속도, 용량
- 테넌트 임대료, 유지비
- 별점 기준 수치
- 타임 스케일 배속 값
- 층당 타일 수, 타일 크기(px)

---

## 타일 그리드 좌표계 ✅ 확정

| 항목 | 값 |
|---|---|
| 1타일 크기 | 32 × 32 px |
| 층고 | 32 px (1타일 = 1층) |
| 원점 | 좌하단 (0, 0) |
| Y축 방향 | 위로 증가 (floor 0 = 지상층) |
| 좌표계 기준 | SimTower 기준 + 픽셀아트 표준 |

```cpp
// DESIGN DECISION: tile = 32x32px, origin = bottom-left, Y increases upward.
// Matches SimTower convention. See VSE_Architecture_Review.md.
struct TileCoordinate { int x; int floor; }; // floor 0 = ground
```

---

## 스프라이트 / 아트 스타일 ✅ 확정

| 항목 | 값 |
|---|---|
| 스타일 | 픽셀아트 (SimTower 느낌) |
| 타일 해상도 | 32 × 32 px |
| NPC 스프라이트 | 16 × 32 px (타일 너비의 절반, 높이 1층) |
| AI 생성 기준 | 32px 그리드 기반, 제한 팔레트 권장 (16~32색) |
| 렌더 배율 | 1x (논리 px = 화면 px), HiDPI 대응은 Phase 2 |

---

## CI/CD 정책 ✅ 확정

- **도구:** GitHub Actions
- **트리거:** main/dev 브랜치 push 시 자동 실행
- **최소 체크:** macOS 빌드 성공 여부
- **설정 파일:** `.github/workflows/build.yml` (Phase 1 시작 시 생성)

---

## 라이선스

> 검토 중
- Layer 0: MIT 라이선스 (오픈소스 배포 가능)
- Layer 1+: 상업 라이선스
- 결정 후 이 항목 업데이트 필요

---

## Phase 2 슬롯 (코드 주석으로만 존재)

```cpp
// [PHASE-2] IIoTAdapter: BACnet/IP 연동 예정
// 스레드 격리 + 비동기 큐 패턴 사용 예정
// MockBACnetDevice로 먼저 테스트 후 실장비 연동
```

```cpp
// [PHASE-3] INetworkAdapter: 권위 서버 모델 예정
// IAuthority + IReplicationManager 계층 추가 예정
```

---

## 붐(PM)이 구현 지시 시 필수 포함 항목

DeepSeek / Claude Code에게 태스크를 줄 때 반드시 포함:

1. 구현 대상 레이어 명시 (Layer 0 / 1 / 2 / 3)
2. 관련 인터페이스명
3. Phase 1 범위 내 여부 확인
4. 단위 테스트 생성 요청 포함
5. 네이밍 컨벤션 준수 요청

---

## 변경 이력

| 날짜 | 버전 | 내용 |
|---|---|---|
| 2026-03 | 1.0 | 크로스 검증 완료 후 최초 작성. |
| 2026-03 | 1.1 | 미결 4개 전부 확정. 타일 좌표계·스프라이트·CI/CD·에러핸들링 추가. |

---
*이 파일의 내용을 변경하려면 반드시 Human 승인 후 붐(PM)이 업데이트한다.*
