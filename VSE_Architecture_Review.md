# VSE Architecture Review & Cross-Validation Log
> Last updated: 2026-03 | Version: 0.5 | Status: 크로스 검증 전체 완료 → CLAUDE.md 작성 단계

---

## 크로스 검증 진행 현황

| 모델 | 상태 | 주요 기여 | 특이 관점 |
|---|---|---|---|
| GPT-4o/5.3 | ✅ 완료 | MVP 스코프 축소 권고, Tick 우선 결정 강조 | 기존 지적 재확인 수준. 새 내용 적음. |
| Gemini | ✅ 완료 | EventBus 네이밍 확정, 메모리 소유권 컨벤션, BAS 시간 동기화 정책 | DOD 전면 도입 권고 (제외) |
| DeepSeek | ✅ 완료 | Layer 0 파손 지점 구체화, 누락 모듈 발굴 | C++17 solo 현실성 회의적 (제외) |
| Opus | ✅ 완료 | 실행 가능성 직격, Phase별 구현 범위 구체화, 수치 제시 | "지금 프로토타입 만들어라" — 가장 실질적 기여 |

> **✅ 크로스 검증 완전 종료. 다음 단계: CLAUDE.md 작성 → Phase 1 구현 시작**
> 추가 설계 논의는 수확 체감 구간. 지금은 코드를 써야 할 시점.

---

## 확정 결정 사항 (크로스 검증 후 최종)

### 즉시 결정 완료

| # | 결정 사항 | 결정 내용 | 근거 |
|---|---|---|---|
| 1 | SimClock Tick | **100ms (10 TPS)** | Opus: 솔로 개발 최적화 부담. 배속은 tick당 스텝 수로 조절. |
| 2 | ECS 라이브러리 | **EnTT** | Opus: 커스텀 ECS는 시간 낭비. Header-only C++17, 문서 풍부. |
| 3 | Serialization | **MessagePack** | Opus: FlatBuffers 스키마 관리 부담. 싱글플레이어 세이브 용도. 네트워크 직렬화는 Phase 3 재검토. |
| 4 | Phase 1 타겟 플랫폼 | **macOS + Steam only** | Opus: iOS/Android 빌드 환경 관리 비용 Phase 1에서 낭비. |
| 5 | Phase 1 엘리베이터 알고리즘 | **LOOK + 2~4대** | Opus: Destination Dispatch는 v1.1 이후. MVP에서 구현 난이도 과도. |
| 6 | Phase 1 EventBus 구현 | **단일 EventBus + replicable 플래그** | Opus: 싱글플레이어에서 StateEventBus FlatBuffers 직렬화 불필요. 듀얼 채널은 설계만 유지. |

### Phase 1 MVP 스코프 (확정)

| 항목 | 제한 | 비고 |
|---|---|---|
| 층수 | 30층 이하 | Opus 제시 |
| NPC | 50명 이하 | Opus 제시 |
| 엘리베이터 | 1축 2~4대, LOOK 알고리즘 | Destination Dispatch는 v1.1 |
| 테넌트 종류 | 3종 (사무실, 주거, 상업) | |
| 경제 | 기본 수입/지출 | 부채/인플레이션 제외 |
| 재난 | 없음 | v1.1 추가 |
| 타겟 플랫폼 | macOS + Steam | iOS/Android Phase 2 |
| Dear ImGui | 디버그 용도만 | |

---

## 반영 확정 항목 (누적)

### Layer 0 모듈 추가/변경

| 모듈 | 출처 | Phase 1 구현 | 설명 |
|---|---|---|---|
| IAsyncLoader | DeepSeek | ✅ 포함 | 리소스 비동기 로딩 |
| IMemoryPool | DeepSeek | ✅ 포함 | NPC/타일 오브젝트 할당 최적화 |
| SpatialPartitioning | Gemini/Opus | ⚠️ 선언만 | NPC 50명 이하면 brute-force 충분. 성능 실측 후 구현. |
| ISpatialRule | 원본 | ❌ Phase 2 | 크루즈/우주선 도메인은 먼 미래 |
| IIoTAdapter | 원본 | ❌ Phase 2 | Phase 1 게임에 BAS 코드 0줄. 로드맵에만 기록. |
| INetworkAdapter + IAuthority | 원본 | ⚠️ 선언만 | 메서드 시그니처 비워둠. Phase 3 구현. |
| AuditLogger | 원본 | ❌ Phase 3 | spdlog 수준으로 충분. Phase 3 온라인 모드에서 필요. |
| VariableEncryption | 원본 | ❌ 제거 | 싱글플레이어 MVP에서 불필요. Steam 출시 후 재검토. |

### Layer 3 추가

| 모듈 | 출처 | Phase 1 구현 | 설명 |
|---|---|---|---|
| IAudioCommand | DeepSeek | ✅ 포함 | BGM/효과음/환경음 분리. SDL2_mixer 기반. |

### EventBus 설계

| 항목 | 내용 |
|---|---|
| 네이밍 | InternalEvent / ExportableEvent (Gemini 확정) |
| Phase 1 구현 | 단일 EventBus + `replicable` 플래그 (Opus) |
| Phase 3 구현 | 듀얼 채널 (LocalEventBus / StateEventBus) + [Networked] 태깅 |

### CLAUDE.md 필수 포함 항목

| 항목 | 출처 | 내용 |
|---|---|---|
| 성능 목표 수치 | DeepSeek/Opus | 60fps, NPC 50명 이하, 30층 이하 |
| ContentRegistry 핫 리로드 | DeepSeek | balance.json 재시작 없이 반영 |
| 메모리 소유권 컨벤션 | Gemini | unique_ptr/shared_ptr/raw pointer 사용 기준 |
| Fixed Time-step | Gemini | 시뮬(100ms) / 렌더링 분리 |
| BACnet 설계 주석 | DeepSeek/Opus | Phase 1 코드에 없음. Phase 2 슬롯만 표시. |
| AI 팀 인터페이스 규약 | Opus | 함수 시그니처, 데이터 구조, 네이밍 컨벤션 명시 |
| 에러 핸들링 정책 | Opus | assert vs exception vs error code 결정 필요 |
| MVP 엔티티 상한 | Opus | NPC 50명, 30층, 엘리베이터 4대 이하 |

---

## 미결 사항 (Open Questions)

| # | 질문 | 관련 의견 | 우선순위 |
|---|---|---|---|
| 1 | CI/CD 정책 | Opus/DeepSeek: GitHub Actions 기본 설정이라도 필요. Phase 1 시작 전 결정. | 🔴 높음 |
| 2 | 에러 핸들링 정책 | Opus: assert vs exception vs error code. Phase 1 시작 전 결정. | 🔴 높음 |
| 3 | 타일 그리드 좌표계 | Opus: 1칸=몇px? 층고? SimTower 기준 or 직접 결정. | 🔴 높음 (Phase 1 초반) |
| 4 | 스프라이트 해상도 및 아트 스타일 | Opus: AI 생성 스프라이트 일관성 기준 필요. | 🔴 높음 (Phase 1 초반) |
| 5 | 라이선스 분리 | DeepSeek/Opus: Layer 0 MIT / Layer 1 상업. 합리적. | 🟡 중간 |
| 6 | BAS 시간 동기화 정책 (TimeScaleBridge) | Gemini/Opus: 게임 배속 시 BACnet 데이터 보간. Phase 2 전 결정. | 🟡 중간 (Phase 2 전) |
| 7 | MockBACnetDevice 구현 시점 | Opus: Phase 2 첫 번째 작업. | 🟡 중간 (Phase 2) |
| 8 | SimTower 리버스 엔지니어링 | Opus: 2주 이상 쓰지 말 것. 직접 프로토타입이 더 효율적. | 🟢 낮음 |
| 9 | Phase 3 서버 언어: Go vs Rust? | — | 🟢 낮음 (Phase 3) |
| 10 | Online sync: 권위 서버 vs lockstep? | — | 🟢 낮음 (Phase 3) |

> **해소된 미결 사항 (5개):**
> SimClock Tick → 100ms | ECS → EnTT | Serialization → MessagePack | Phase 1 플랫폼 → macOS+Steam | 엘리베이터 알고리즘 → LOOK

---

## Layer 0 최신 모듈 목록 (Phase 1 기준)

| 모듈 | Phase 1 상태 | 비고 |
|---|---|---|
| IGridSystem | ✅ 구현 | 수평 지원은 추후 |
| IAgentSystem | ✅ 구현 | |
| ITransportSystem | ✅ 구현 | |
| EventBus (단일 + replicable 플래그) | ✅ 구현 | 듀얼 채널은 Phase 3 |
| IEconomyEngine | ✅ 구현 | 기본 수입/지출만 |
| SimClock (100ms) | ✅ 구현 | 렌더링과 분리 |
| LocaleManager (ko/en) | ✅ 구현 | ja/zh는 나중 |
| ContentRegistry | ✅ 구현 | 핫 리로드 포함 |
| ConfigManager | ✅ 구현 | |
| ISaveLoad | ✅ 구현 | MessagePack. 버전 번호만. Migration은 추후. |
| Bootstrapper | ✅ 구현 | |
| IAsyncLoader | ✅ 구현 | |
| IMemoryPool | ✅ 구현 | |
| SpatialPartitioning | ⚠️ 선언만 | 성능 실측 후 구현 |
| INetworkAdapter + IAuthority | ⚠️ 선언만 | 메서드 비워둠. Phase 3. |
| IIoTAdapter | ❌ 미포함 | Phase 2. 로드맵만. |
| ISpatialRule | ❌ 미포함 | Phase 2+. |
| AuditLogger | ❌ 미포함 | Phase 3. spdlog로 대체. |
| VariableEncryption | ❌ 제거 | 재검토: Steam 출시 후. |

---

## 제외 항목

| 항목 | 출처 | 제외 이유 |
|---|---|---|
| C++17 → Rust/Bevy/Godot 전환 | DeepSeek/Opus | 스택 확정 완료. 논의 종결. |
| EconomyEngine 재정 모델 (부채/이자) | DeepSeek | Phase 2 항목. |
| ITelemetry | DeepSeek | Phase 2+ 항목. |
| DOD 전면 전환 | Gemini | ECS가 이미 DOD 핵심 이점 제공. |
| FlatBuffers (세이브 포맷) | Gemini 제안 → Opus 반론 | 스키마 관리 부담. MessagePack으로 결정. |
| Destination Dispatch (Phase 1) | Opus | v1.1 이후. MVP는 LOOK. |
| VariableEncryption (Phase 1) | Opus | 싱글플레이어 불필요. |
| AuditLogger (Phase 1) | Opus | spdlog 충분. Phase 3. |
| Deterministic Simulation 완전 보장 | Opus | Phase 1 목표로 하되 보장 안 함. Phase 3 테스트. |

---

## Phase 2 TODO (미리 기록)

| 항목 | 출처 | 내용 |
|---|---|---|
| IIoTAdapter 구현 | 원본 | BACnet/IP 연동. 스레드 격리 + 비동기 큐. |
| MockBACnetDevice | Opus | Phase 2 첫 번째 작업. 실제 장비 없이 테스트 가능. |
| TimeScaleBridge | Opus/Gemini | 게임 배속 ↔ BACnet 실시간 시간 매핑 컴포넌트. |
| BACnet 범위 한정 | Opus | BACnet/IP 시뮬레이션 연동만. MS/TP, SC는 추후. |
| iOS/Android 포팅 | Opus | Phase 1 macOS+Steam 완료 후. |

---

## 변경 이력

| 날짜 | 버전 | 내용 |
|---|---|---|
| 2026-03 | 0.1 | DeepSeek 추가 답변 검토. 초기 문서 생성. |
| 2026-03 | 0.2 | Gemini 답변 검토 반영. |
| 2026-03 | 0.3 | MD 파일 관리 방식 전환. |
| 2026-03 | 0.4 | Opus 답변 검토 완료. 미결 5개 해소. |
| 2026-03 | 0.5 | GPT-5.3 답변 검토. 신규 반영 없음. 크로스 검증 전체 완료. |

---
*다음 단계: CLAUDE.md 작성 → Phase 1 구현 시작*
