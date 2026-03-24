# VSE Platform — 프로젝트 개요
> 용도: 새 세션 시작 시 AI에게 줄 압축 컨텍스트
> 상세 내용이 필요하면 VSE_Session_Summary.md 참조

---

## 지금 어디까지 왔나

| 단계 | 상태 |
|---|---|
| 기획 / 크로스 검증 (GPT·Gemini·DeepSeek·Opus) | ✅ 완료 |
| 기술 스택 / 아키텍처 / 운영 규칙 확정 | ✅ 완료 |
| 미결 사항 전부 해소 | ✅ 완료 |
| **Phase 1 구현 시작** | ⏳ **← 지금 여기** |

---

## 프로젝트 정체성

**Tower Tycoon** — SimTower 스타일 빌딩 관리 시뮬레이션
장기 비전: 게임 로직 ↔ BAS(빌딩 자동화 시스템) 1:1 매핑 → 교육용 디지털 트윈
개발자: 솔로, 한국, 비개발자 + 바이브코딩 + AI 멀티 에이전트

---

## 확정 스택

`C++17` · `SDL2` · `Dear ImGui` · `EnTT` · `CMake` · `MessagePack` · `SQLite` · `nlohmann/json`
배포: **macOS + Steam** (Phase 1) — iOS/Android는 Phase 2

---

## Phase 1 MVP 범위

**포함:** 타일 그리드(30층) · NPC(50명) · 엘리베이터 LOOK(2~4대) · 테넌트 3종 · 기본 경제 · 별점
**절대 제외:** BACnet · 멀티플레이어 · 재난 · iOS/Android · Destination Dispatch · 부채/인플레이션

---

## 아키텍처

```
Layer 0  인터페이스만     구체 구현 절대 금지
Layer 1  게임 도메인      RealEstateDomain — 로직 전담
Layer 2  콘텐츠 팩        JSON + 스프라이트
Layer 3  SDL2 렌더러       IRenderCommand — Dirty Rect
```

---

## 핵심 확정 수치

| 항목 | 값 |
|---|---|
| SimClock Tick | 100ms (10 TPS) |
| 타일 크기 | 32 × 32 px, 좌하단 원점, Y축 위로 |
| 스프라이트 | 픽셀아트 32×32, 팔레트 16~32색 |
| 렌더링 목표 | 60fps |
| 에러 핸들링 | assert + error code, exception 금지 |
| CI/CD | GitHub Actions, macOS 빌드 자동 확인 |

---

## AI 팀

| 역할 | 모델 | 담당 |
|---|---|---|
| PM | 붐 (Sonnet / 설계시 Opus) | 지시 조율·컨펌·에스컬레이션 |
| 개발 | DeepSeek | 게임 로직·알고리즘 |
| 렌더링 | Claude Code | SDL2 Layer 3 |
| QA | GPT-4o | 코드 품질 리뷰 |
| QA | Gemini 2.5 Flash | 통합 테스트 시나리오 |

Human ↔ 붐: **한국어** / 붐 → 개발팀: **영어**

---

## 관리 파일

| 파일 | 용도 |
|---|---|
| `CLAUDE.md` | 구현 판단 최우선 기준. 모든 AI 필독. |
| `VSE_Session_Summary.md` | 세션 상세 컨텍스트 (결정 근거 포함) |
| `VSE_AI_Team.md` | 팀 운영 규칙 전체 |
| `VSE_Architecture_Review.md` | 크로스 검증 기록 |
| `VSE_Sprint_Status.json` | 대시보드 데이터 (붐이 업데이트) |
| `VSE_Dashboard.html` | 웹 대시보드 (JSON과 같은 폴더에서 열기) |

---

## 다음 액션

```
1. git init + 첫 커밋
2. 붐(Opus 모드)에게 Sprint 1 태스크 분해 지시
3. 구현 — 타일 그리드 + NPC 이동 프로토타입 (2주 목표)
```

---

## 이 파일을 받은 AI에게

1. 이 파일과 `CLAUDE.md`를 읽었다면 컨텍스트 파악 완료
2. "다음 액션" 중 어느 단계인지 Human에게 먼저 확인
3. 모르는 것은 추측 말고 Human에게 질문
4. 상세 내용 필요 시 `VSE_Session_Summary.md` 요청
