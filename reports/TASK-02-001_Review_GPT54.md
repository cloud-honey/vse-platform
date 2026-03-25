# VSE Platform 검토 결과 — GPT-5.4 Thinking

## 검토 대상
- 문서: `TASK-02-001 Bootstrapper 클래스 분리 (main.cpp 리팩토링)` 작업 결과 보고서
- 검토 기준: 첨부 보고서 + `CLAUDE.md` + `VSE_Design_Spec.md`
- 검토 방식: **보수적 검토**
- 전제: 이번 평가는 **실제 전체 소스코드가 아니라 첨부 보고서와 기준 문서의 정합성 대조 중심**이다. 따라서 아래 평가는 “구현이 잘못되었다”의 단정이 아니라, **현재 보고서 기준으로 확인되는 설계 적합성 / 불확실성 / 잠재 리스크**를 정리한 것이다.

---

## 한 줄 결론
**리팩토링 방향 자체는 맞지만, 현재 보고서 기준으로는 “Bootstrapper 분리 성공”까지는 인정 가능하나, “설계대로 정확히 구현되었다”라고 확정하기에는 이르다.**

판정: **조건부 합격**

- `main.cpp`를 Composition Root 형태로 단순화한 방향은 적절하다.
- 다만 기준 문서가 전제한 **SimClock 책임 구조**, **Bootstrapper 소유 범위**, **초기화/종료 계약**, **테스트 경계**와는 어긋나거나 불명확한 지점이 남아 있다.
- 따라서 이번 태스크는 “리팩토링 1차 성공”으로 볼 수는 있어도, “아키텍처 정합성까지 완전히 확보된 최종형”으로 보기는 어렵다.

---

## 잘 된 점

### 1) `main.cpp` 단순화 방향은 맞다
보고서상 변경 후 `main()`은 `Bootstrapper` 생성 → `init()` → `run()` → `shutdown()`만 수행한다. 이 자체는 엔트리 포인트를 얇게 만들고 시스템 조립 책임을 한 곳으로 모으는 좋은 방향이다.

### 2) Composition Root 의도는 맞다
초기화 순서, 시스템 조립, 메인 루프 캡슐화를 `Bootstrapper`에 모은 것은 기준 문서의 의도와 대체로 맞는다. 특히 “main에 흩어진 초기화/루프/커맨드 처리 정리”라는 목적 자체는 타당하다.

### 3) 테스트 가능한 진입점 확보는 실용적이다
`initDomainOnly()`를 둬서 SDL 없는 headless 테스트가 가능해진 것은 실무적으로 유용하다. UI/SDL 의존성을 분리해 핵심 동작을 테스트하려는 시도 자체는 좋은 설계 습관이다.

### 4) 리팩토링 범위를 크게 넘어서지 않은 점은 긍정적이다
보고서 기준으로는 기존 기능을 옮기고 루프를 정리하는 쪽에 집중했으며, 새로운 게임 규칙을 추가하지는 않았다. 이 점은 Sprint 2 첫 작업으로 적절하다.

---

## 핵심 우려점

## 1. 가장 큰 문제: 시간 소유권이 SimClock에서 Bootstrapper로 이동한 흔적
이번 태스크에서 가장 크게 보이는 우려는 이것이다.

보고서의 `run()` 설명은 다음 구조를 전제로 한다.
- `accumulator_ += realDeltaMs * speed_`
- `while (accumulator_ >= tickMs_)`
- `GameTime::fromTick(currentTick_)`
- `currentTick_++`
- `eventBus_.flush()`

즉, **Bootstrapper가 직접 tick 누적, 시간 계산, 배속, tick 진행을 관리하는 형태**로 읽힌다.

그런데 기준 문서는 다르다.
- `SimClock`이 100ms 고정 tick을 소유한다.
- `SimClock::update(realDeltaMs)`가 시간 진행의 중심이다.
- `Bootstrapper`는 이를 호출하고, 시스템 갱신을 오케스트레이션하는 역할이다.

이 차이는 단순 구현 스타일 차이가 아니다. **시간 시스템의 책임 주체가 바뀐 것**이다.

보수적으로 보면 이 문제는 다음 위험으로 이어진다.
- 추후 저장/불러오기 시 시간 상태 복원 책임이 애매해질 수 있다.
- 배속/일시정지 상태가 SimClock와 Bootstrapper 양쪽에 중복될 수 있다.
- TickAdvanced 이벤트 발행 시점과 실제 시스템 업데이트 순서가 기준 문서와 어긋날 수 있다.
- “SimClock이 존재하지만 사실상 우회되는 구조”가 되면, 이후 시스템들이 시간을 바라보는 기준이 흔들릴 수 있다.

**판단:**
이번 태스크가 단순 리팩토링이어야 했다면, 시간 누적과 tick 진행 로직은 가능하면 `SimClock`에 그대로 남기고 `Bootstrapper`는 orchestration만 담당했어야 더 안전했다.

---

## 2. Bootstrapper 인터페이스가 설계 문서 계약과 다르게 흘러간다
보고서의 인터페이스는 다음 특성이 있다.
- `init()` / `run()` / `shutdown()`
- `initDomainOnly()`
- `testGetPaused()`, `testGetSpeed()`, `testGetRunning()`
- `testProcessCommands()`

반면 기준 문서는 대체로 다음에 가깝다.
- `initialize(configPath)`
- `run()`은 종료 코드를 반환
- 시스템 소유는 보다 넓은 범위를 포함
- 테스트 전용 API 공개는 기본 전제가 아님

이 차이를 무조건 오류라고 보기는 어렵다. 다만 보수적으로 보면 다음 문제가 있다.

### 2-1) 프로덕션 API에 테스트 전용 표면이 노출된다
`testGet*()`와 `testProcessCommands()`가 public으로 노출되면, 프로덕션 객체가 테스트 편의를 위해 구조적으로 오염된다.

이 문제는 단순 미관 문제가 아니다.
- 외부 코드가 이 API를 실제로 호출할 수 있다.
- 이후 리팩토링 시 “테스트가 붙어 있으니 public 유지”라는 식으로 설계가 굳어질 수 있다.
- 테스트 seam이 클래스 경계를 왜곡한다.

**보수 판단:**
이 부분은 보고서가 스스로 미결 항목으로 적어 둔 것처럼, 반드시 `VSE_TESTING` 가드나 friend test, 별도 테스트 훅 구조로 정리하는 편이 안전하다.

### 2-2) `initDomainOnly()`는 유용하지만 Composition Root를 흐릴 수 있다
`Bootstrapper`는 본래 “실행 시스템 조립자”에 가까운 클래스다. 그런데 여기에 테스트용 조립 경로까지 강하게 넣으면, 점점 “앱 실행기 + 테스트 픽스처 + 샘플 씬 생성기”가 한 클래스에 섞일 가능성이 있다.

즉 지금은 편리하지만, 장기적으로는 아래처럼 분리되는 편이 더 안정적이다.
- `Bootstrapper`: 실제 앱 초기화/실행/종료
- `DomainAssembly` 또는 `AppFactory`: 테스트 가능한 조립 책임
- `SceneSeeder` 또는 `InitialSceneBuilder`: 초기 씬 구성 책임

---

## 3. Bootstrapper가 ‘모든 시스템’을 실제로 소유하는지 보고서만으로는 불명확하다
기준 문서의 Bootstrapper는 EventBus, Config, ContentRegistry, LocaleManager, SimClock, Grid/Agent/Transport, Economy, SaveLoad, Renderer, DebugPanel까지 포함하는 상위 조립자에 가깝다.

그런데 이번 보고서의 private 멤버 예시는 다음에 집중되어 있다.
- ConfigManager
- EventBus
- registry
- GridSystem
- AgentSystem
- TransportSystem
- SDLRenderer
- Camera
- InputMapper
- RenderFrameCollector
- 속도/정지/tick 관련 상태

여기서 빠져 보이는 것들이 있다.
- SimClock
- ContentRegistry
- LocaleManager
- EconomyEngine
- StarRatingSystem
- SaveLoad
- DebugPanel

물론 보고서가 모든 멤버를 다 적지 않았을 가능성도 있다. 그래서 이것을 “미구현”이라고 단정할 수는 없다.

하지만 보수적으로는 다음 해석이 가능하다.
- **문서화 누락**일 수 있다.
- 혹은 아직 Bootstrapper가 Phase 1/2 전체 시스템 조립자 역할까지는 올라오지 못했을 수 있다.

어느 쪽이든 문제는 남는다. 왜냐하면 Bootstrapper 태스크 보고서라면, **무엇을 조립하고 무엇을 아직 조립하지 않는지 경계가 매우 명확해야 하기 때문**이다.

**판단:**
이번 보고서는 “main 분리 성공”은 전달하지만, “아키텍처 상 최상위 조립자 역할을 어디까지 담당하는지”는 충분히 증명하지 못한다.

---

## 4. shutdown null guard는 크래시 방지에는 도움이 되지만, 종료 계약이 완전히 안전해졌다고 보기 어렵다
보고서의 수정은 `renderer_` null guard를 추가해 SDL 미초기화 상태의 assert를 피하는 것이다.

이 수정은 분명 필요했다. 다만 보수적으로 보면 이건 **크래시 방지 패치**이지, 곧바로 **완전한 생명주기 정합성 확보**를 뜻하지는 않는다.

남는 의문은 다음과 같다.
- ImGui context가 항상 `renderer_` 존재 여부와 1:1로 대응하는가?
- partial init 실패 시 `window_`만 있고 `renderer_`는 없을 수 있는가?
- 중복 `shutdown()` 호출이 완전히 무해한가?
- destructor와 명시적 `shutdown()`의 이중 경로가 항상 안전한가?

즉 현재 패치는 “바로 터지던 assert”는 막았지만, **부분 초기화 실패 경로와 중복 종료 경로를 체계적으로 검증했다는 근거는 부족**하다.

**권고:**
`initialized_`, `imguiInitialized_`, `sdlInitialized_`처럼 명시적인 lifecycle 플래그를 두는 쪽이 더 견고하다.

---

## 5. 테스트 5개는 기본 동작 확인용으로는 충분하지만, 아키텍처 리팩토링 검증용으로는 얕다
이번 테스트는 다음을 검증한다.
- initDomainOnly 성공
- pause 토글
- speed 변경
- quit 처리
- BuildFloor 시 crash 없음

이건 “기본 조작이 된다”는 확인으로는 괜찮다. 하지만 보수적으로 보면, 이번 태스크의 위험 지점은 여기보다 깊다.

이번 리팩토링에서 진짜 중요한 테스트는 오히려 아래에 더 가깝다.
- `init()` 실패 중간 단계에서 `shutdown()`이 안전한가
- `shutdown()` 2회 호출이 안전한가
- `run()`이 paused/speed 변화에 따라 tick 수를 올바르게 제한하는가
- `MAX_TICKS_PER_FRAME`가 실제로 spiral-of-death를 막는가
- `processCommands()`가 frame당 1회인지, tick당 1회인지 의도대로 동작하는가
- `SimClock`가 별도로 있다면 그 상태와 Bootstrapper 상태가 충돌하지 않는가
- `RenderFrameCollector`가 Domain 읽기 전용 경계를 지키는가

즉 현재 테스트는 “작동 확인”에 가깝고, **리팩토링 안정성 검증**으로는 아직 부족하다.

---

## 6. `processCommands()` 처리 위치가 애매하다
보고서도 스스로 “이중 호출 가능성”을 미결 항목으로 적어 두었다. 이건 작은 메모가 아니라 실제로 중요한 설계 문제다.

이유는 다음과 같다.
- 입력 수집은 frame 기준이다.
- 시뮬레이션 업데이트는 tick 기준이다.
- 이 둘이 섞이면 입력 지연, 중복 반영, 배속 시 입력 체감 차이 같은 문제가 나온다.

특히 보고서에는
- `handleInput()`에서 commands를 만든다
- tick 루프 첫 tick에서만 `processCommands()`를 한다
- 그런데 “run()의 커맨드 처리와 tick 루프 내 processCommands가 의도적으로 분리”되었다는 문장도 있다

이 표현만 놓고 보면, 실제 호출 책임이 한 군데인지 두 군데인지 확신하기 어렵다.

**보수 판단:**
이 부분은 반드시 문서와 코드 주석에서 아래 둘 중 하나로 못 박아야 한다.
1. 입력은 frame에서 수집하고, 반영은 다음 simulation step 첫 tick에서 정확히 1회 처리한다.
2. 입력은 frame 수집 즉시 처리하되, 시뮬레이션 상태 변경은 tick 경계에 큐잉한다.

지금 상태는 구조적으로 맞을 수도 있지만, **읽는 사람이 오해하기 쉬운 형태**다. 이런 애매함은 이후 버그의 출발점이 된다.

---

## 7. `initDomainOnly()`에 초기 씬 설정이 중복된 것은 작지만 분명한 설계 부채다
보고서가 이미 인정한 부분이다. `init()`과 `initDomainOnly()` 모두 5층/NPC 등 초기 씬 설정 코드를 가진다면, 다음 문제가 생긴다.
- 실제 실행 경로와 테스트 경로가 서서히 달라질 수 있다.
- 한쪽만 수정되고 다른 쪽이 잊힐 수 있다.
- 테스트가 “실제 앱과 동일한 조립”을 검증하지 못하게 된다.

이건 당장 큰 문제는 아니지만, Bootstrapper 도입 직후 단계에서 정리해야 나중에 커지지 않는다.

---

## 8. `VSE_PROJECT_ROOT` 매크로 의존은 테스트 편의에는 도움 되지만, 코어 런타임의 경로 독립성을 약화시킨다
`initDomainOnly()`가 프로젝트 루트를 컴파일 타임 매크로에 의존하는 구조는 테스트나 실행 편의에는 좋다. 하지만 보수적으로 보면 다음 우려가 있다.
- 코어 런타임이 빌드 시스템 세부사항을 더 많이 알게 된다.
- 타깃 추가/분리 시 매크로 누락으로 재발할 수 있다.
- 향후 번들 배포, 설치 경로 변경, 플랫폼 확장 시 경로 해석이 취약해질 수 있다.

즉 이번 수정은 실용적이지만, **근본적으로는 자산 경로 해결 전략이 앱 구성 계층에 더 명확히 분리되어야 한다**는 신호다.

---

## 설계 대비 세부 판정

| 항목 | 판단 | 코멘트 |
|---|---|---|
| `main.cpp` 단순화 | 적합 | 이번 태스크 목적과 잘 맞음 |
| Composition Root 방향 | 적합 | 방향은 맞지만 책임 범위가 아직 다소 혼합됨 |
| SimClock 책임 유지 | 우려 | 보고서상 Bootstrapper가 tick/시간을 직접 관리하는 형태로 읽힘 |
| Bootstrapper 시스템 소유 범위 | 불명확 | 설계 문서 전체 범위 대비 보고서 설명이 좁음 |
| 테스트 전용 API 노출 | 우려 | public 표면 오염 가능성 큼 |
| shutdown 안정성 | 부분 개선 | 크래시 방지는 했지만 partial init / double shutdown 검증은 부족 |
| frame/tick 경계 명확성 | 우려 | `processCommands()` 처리 위치가 문서상 애매함 |
| 테스트 깊이 | 부족 | 동작 확인 위주, 리팩토링 안정성 검증은 얕음 |
| 장기 유지보수성 | 보통 | 지금은 작동하나 책임 분리 미세 조정 필요 |

---

## 최종 판단

### 최종 판정: 조건부 합격
이번 태스크는 **“main.cpp 리팩토링”이라는 1차 목표에는 대체로 성공**했다.

하지만 보수적으로 보면, 아직 아래 질문에 명확한 답이 부족하다.
- 시간 진행의 단일 진실 공급원은 SimClock인가, Bootstrapper인가?
- Bootstrapper는 어디까지 조립하고 어디부터는 다른 조립기/팩토리가 맡는가?
- 테스트 편의를 위해 열린 public API를 어떻게 다시 닫을 것인가?
- 부분 초기화 실패와 종료 경로는 정말 안전한가?
- 입력 처리와 tick 처리의 경계가 코드와 문서에서 동일하게 이해되는가?

즉, **“리팩토링 완료”는 가능하지만 “설계 정합성 완전 확보” 판정은 아직 보류**가 맞다.

---

## 우선순위 권고

### P0
1. **SimClock와 Bootstrapper의 책임 경계 재확정**
   - 시간 누적, 배속, pause, currentTick의 단일 소유자를 명확히 정리
   - 문서와 코드가 동일한 구조를 갖도록 맞출 것

2. **`processCommands()` 호출 계약 문서화 + 테스트 추가**
   - frame 기준 1회인지, tick 기준 첫 step 1회인지 명확히 못 박을 것

### P1
3. **테스트 전용 public API 제거 또는 가드 처리**
   - `#ifdef VSE_TESTING`, friend test, internal accessor 등으로 축소

4. **`setupInitialScene()` 추출**
   - `init()` / `initDomainOnly()` 중복 제거

5. **partial init / double shutdown 테스트 추가**
   - 이번 태스크에서 가장 실제적인 안정성 확보 포인트

### P2
6. **자산 경로 해석 책임 분리**
   - `VSE_PROJECT_ROOT` 의존을 장기적으로 더 견고한 경로 해석 구조로 이동

---

## 핵심 한 줄 판단
**이번 작업은 “main을 정리한 리팩토링”으로서는 성공이지만, “설계 문서가 의도한 Bootstrapper 최종 구조”로 보기에는 시간 책임과 테스트 경계가 아직 흔들린다.**
