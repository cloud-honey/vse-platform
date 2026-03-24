# VSE Platform — 세션 요약
> 작성: 2026-03 | 용도: 새 세션 시작 시 컨텍스트 복원용
> **이 파일 + CLAUDE.md 두 개만 주면 새 세션에서 바로 이어서 작업 가능**

---

## 한 줄 요약

SimTower 스타일 빌딩 관리 게임 **Tower Tycoon**을 C++17+SDL2로 만들고 있음.
4개 AI 크로스 검증 완료. 설계 확정. 다음 단계는 **CLAUDE.md 기반 Phase 1 구현 시작**.

---

## 프로젝트 정체성

- **이름:** VSE Platform — Tower Tycoon (Phase 1)
- **장르:** SimTower 스타일 빌딩 관리 시뮬레이션
- **장기 비전:** 게임 로직 ↔ BAS(빌딩 자동화 시스템) 1:1 매핑. 교육용 BAS 디지털 트윈.
- **시장 차별점:** 게이미파이드 BAS 시뮬레이터. 아직 세상에 없음.
- **개발자:** 솔로, 한국, 비개발자 배경 + 바이브코딩 + AI 멀티 에이전트

---

## 확정 기술 스택

| 항목 | 결정 | 근거 |
|---|---|---|
| 언어 | C++17 | 확정. 변경 논의 종결. |
| 그래픽 | SDL2 + SDL2_image + SDL2_ttf | |
| UI | Dear ImGui (디버그 전용, Phase 1) | |
| ECS | **EnTT** (Header-only) | Opus: 커스텀 ECS 금지 |
| 빌드 | CMake | |
| JSON | nlohmann/json | |
| 직렬화 | **MessagePack** | Opus: FlatBuffers 스키마 부담 > MessagePack |
| 로컬 DB | SQLite | |
| BAS | bacnet-stack → Phase 2 | Phase 1에 BAS 코드 0줄 |
| 배포 | macOS + Steam (Phase 1) | iOS/Android는 Phase 2 |

---

## AI 팀 구성

```
[Human] 최종 판단 / 버그 진단 / 게임 필 / 단계별 컨펌
   ↕ 한국어
[붐 Boom] PM (기본: Sonnet / 설계 시: Opus)
   ↕ 영어 (지시)
   ├─ [DeepSeek]        개발 — 게임 로직 / 알고리즘
   ├─ [Claude Code]     개발 — SDL2 렌더링 Layer 3
   ├─ [GPT-4o]          QA — 코드 품질 리뷰
   └─ [Gemini 2.5 Flash] QA — 통합 테스트 시나리오
```

**붐 운영 원칙:**
- Human↔붐 = 한국어 / 붐→팀 = 영어
- 태스크 완료마다 Human 컨펌 후 다음 진행
- CLAUDE.md에 없는 결정 임의로 내리지 않음
- 설계 오류 발견 즉시 중단 + 에스컬레이션

---

## Phase 1 MVP 확정 스코프

| 항목 | 제한 |
|---|---|
| 층수 | 30층 이하 |
| NPC | 50명 이하 |
| 엘리베이터 | 1축 2~4대, LOOK 알고리즘 |
| 테넌트 | 3종 (사무실/주거/상업) |
| 경제 | 기본 수입/지출 |
| 재난 | 없음 (v1.1 추가) |
| 플랫폼 | macOS + Steam |
| Dear ImGui | 디버그 전용 |

**Phase 1에 없어야 할 것:** BACnet, 멀티플레이어, iOS/Android, VariableEncryption, AuditLogger, Destination Dispatch, 부채/인플레이션

---

## 크로스 검증 완료 — 핵심 결정 5개 (Opus 기준)

| # | 결정 | 내용 |
|---|---|---|
| 1 | SimClock Tick | 100ms (10 TPS) |
| 2 | ECS | EnTT |
| 3 | Serialization | MessagePack |
| 4 | Phase 1 플랫폼 | macOS + Steam only |
| 5 | 엘리베이터 알고리즘 | LOOK (Destination Dispatch는 v1.1) |

---

## 아키텍처 4-Layer 요약

```
Layer 0  VSE Core       인터페이스만. 구체 구현 절대 금지.
Layer 1  Domain Module  RealEstateDomain. 게임 로직.
Layer 2  Content Pack   JSON + 스프라이트. 코드 없이 DLC 가능.
Layer 3  SDL2 Renderer  IRenderCommand 기반. Dirty Rect.
```

**Phase 1 Layer 0 구현 목록 (요약):**
- ✅ 구현: IGridSystem, IAgentSystem, ITransportSystem, EventBus, IEconomyEngine, SimClock, LocaleManager, ContentRegistry, ConfigManager, ISaveLoad, Bootstrapper, IAsyncLoader, IMemoryPool
- ⚠️ 선언만: SpatialPartitioning, INetworkAdapter+IAuthority
- ❌ 없음: IIoTAdapter, ISpatialRule, AuditLogger, VariableEncryption

---

## 미결 사항 ✅ 전부 해소

| 항목 | 결정 내용 |
|---|---|
| 타일 그리드 좌표계 | 1타일=32px, 층고=32px, 원점=좌하단, Y축 위로 증가 |
| 스프라이트 해상도 & 스타일 | 32×32px 픽셀아트, SimTower 스타일, 팔레트 16~32색 권장 |
| CI/CD | GitHub Actions, macOS 빌드 자동 확인 |
| 에러 핸들링 | assert(개발중) + error code 반환(런타임) + exception 금지 |

> **미결 사항 0개. 설계 완전 완료.**

---

## 생성된 관리 파일 목록

| 파일 | 용도 |
|---|---|
| `CLAUDE.md` | **구현 최우선 기준서.** 모든 AI가 이 파일을 먼저 읽어야 함. |
| `VSE_AI_Team.md` | AI 팀 역할, 언어 규칙, 태스크 분해, 컨펌 절차, git/주석 규칙 |
| `VSE_Architecture_Review.md` | 크로스 검증 누적 기록 (GPT/Gemini/DeepSeek/Opus) |
| `VSE_Sprint_Status.json` | 대시보드 데이터. 붐이 태스크 완료마다 업데이트. |
| `VSE_Dashboard.html` | 프로젝트 웹 대시보드. JSON과 같은 폴더에서 열기. |
| `VSE_Session_Summary.md` | 이 파일. 새 세션 컨텍스트 복원용. |

---

## 다음에 해야 할 것

1. **git 저장소 초기화** — `git init` + 첫 커밋 (CLAUDE.md, VSE_AI_Team.md)
2. **붐(Opus 모드)에게 Phase 1 첫 스프린트 태스크 분해 지시**
3. **구현 시작** — 타일 그리드 + NPC 하나 화면에 띄우기

> ✅ 설계 미결 사항 0개. 바로 구현 진입 가능한 상태.
> Opus 강조: "2주 안에 타일 그리드 + NPC 이동 프로토타입. 안 나오면 완성 확률 급락."

---

## 새 세션에서 이 파일을 받은 AI에게

당신이 지금 이 파일을 받았다면:
1. 이 요약과 CLAUDE.md를 모두 읽었을 것입니다
2. 위 "다음에 해야 할 것" 중 어느 단계에 있는지 Human에게 확인하세요
3. CLAUDE.md가 모든 구현 판단의 기준입니다
4. 모르는 것은 추측하지 말고 Human에게 물어보세요

---
*이 파일은 세션이 끝날 때마다 업데이트하세요.*
