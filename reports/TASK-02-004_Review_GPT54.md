# Cross-Validation Review — TASK-02-004

> Reviewer: GPT-5.4 Thinking
> Verdict: Conditional Pass

## Issues Found

### P0 — Must Fix Before Next Task
- **`IAgentSystem` 계약 드리프트 가능성**
  - 보고서의 공개 API는 `StarRatingSystem::update(entt::registry& reg, const IAgentSystem& agents, const GameTime& time)` 형태이며, Open Items에서는 `IAgentSystem::getAverageSatisfaction()` 시그니처 문제를 직접 언급합니다. 즉 StarRatingSystem이 평균 만족도를 얻기 위해 `IAgentSystem`의 추가 query API에 의존하고 있다는 뜻입니다.
  - 그런데 승인된 `VSE_Design_Spec.md`의 `IAgentSystem` 인터페이스에는 `spawnAgent`, `despawnAgent`, `activeAgentCount`, `update`, `getState`, `getAgentsOnFloor`, `getAgentsInState`만 있고, `getAverageSatisfaction()`는 없습니다.
  - 더 큰 문제는 이번 작업의 수정 파일 목록에도 `include/core/IAgentSystem.h` 변경이 없습니다. 따라서 **(a) Layer 0 인터페이스가 승인 없이 바뀌었거나, (b) 실제 변경 파일이 보고서에서 누락되었거나, (c) StarRatingSystem의 의존 방식이 보고서에 잘못 서술되었을 가능성**이 있습니다.
  - 이건 다음 태스크로 넘어가기 전에 반드시 정리되어야 합니다. Layer 0 계약은 설계 기준서와 구현 보고서가 정확히 일치해야 합니다.

### P1 — Fix Soon
- **임계값 비교에 `-0.0001f`를 넣어 실제 게임 밸런스 수치를 조용히 바꾸고 있음**
  - CLAUDE.md는 별점 기준 수치를 외부 JSON/ConfigManager에서 읽도록 요구하고 있고, Design Spec도 `balance.json`의 `starRating.thresholds`를 기준으로 삼습니다.
  - 그런데 보고서의 epsilon 처리(`avgSatisfaction >= thresholds[i] - 0.0001f`)는 외부 설정값을 그대로 쓰는 것이 아니라, 실질적으로 임계값을 더 낮춘 새 규칙을 코드 안에 추가한 셈입니다.
  - 정밀도 문제를 다루려면 평균 계산 경로를 안정화하거나, 허용 오차를 명시적 정책으로 문서화하는 편이 맞습니다. 지금 방식은 "설정값은 30인데 실제 판정은 29.9999부터"가 되어 기준서와 플레이 밸런스 해석이 어긋날 수 있습니다.

### P2 — Nice to Have
- **`StarRatingChangedPayload`는 공유 타입 성격이 강하므로 `Types.h` 쪽이 더 설계와 잘 맞음**
  - Design Spec은 `include/core/Types.h`를 "Shared types, enums, constants" 위치로 정의하고 있고, `EventType`에도 이미 `StarRatingChanged`가 공용 이벤트로 잡혀 있습니다.
  - 현재처럼 payload를 `include/domain/StarRatingSystem.h`에 두면, 다른 시스템이나 UI가 이벤트를 구독할 때 domain header에 불필요하게 결합됩니다.
  - 즉시 수정이 필요한 수준은 아니지만, 통합 단계 전에 shared type 위치를 정리하는 편이 좋습니다.
