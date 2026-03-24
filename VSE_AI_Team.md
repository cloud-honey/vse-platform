# VSE AI Team — 역할 분담 & 운영 규칙
> Last updated: 2026-03 | Version: 2.0

---

## 팀 구성 개요

```
[Human] 통합 판단 / 버그 진단 / 게임 필 / 최종 결정 / 단계별 컨펌
   │  ↕ 한국어
   ├─ [붐 Boom] PM · 설계 · 지시 조율  (기본: Sonnet / 설계기: Opus)
   │     │  ↕ 영어 (모든 지시)
   │     ├─ [DeepSeek] 개발 (알고리즘 · 구현)
   │     ├─ [Claude Code / Sonnet] 렌더링 구현
   │     └─ [GPT-4o + Gemini 2.5 Flash] QA
   │
   └─ [Claude (여기)] 설계 문서 관리 · 크로스 검증 · CLAUDE.md 작성
```

---

## 언어 규칙 (필수)

| 대화 방향 | 언어 | 이유 |
|---|---|---|
| Human ↔ 붐 | 한국어 | 의사소통 효율 |
| 붐 → DeepSeek | 영어 | 모델 지시 정밀도 |
| 붐 → Claude Code | 영어 | 모델 지시 정밀도 |
| 붐 → GPT-4o | 영어 | 모델 지시 정밀도 |
| 붐 → Gemini | 영어 | 모델 지시 정밀도 |
| 붐 → Human 보고 | 한국어 | 가독성 |

**붐의 영어 지시 필수 포함 항목:**
```
Task ID: TASK-XXX
Layer: [Layer 0 / 1 / 2 / 3]
Scope: [구현 대상 명확히]
Constraints: [CLAUDE.md 관련 규칙 명시]
Expected Output: [결과물 형식]
Unit Test Required: YES
Report back when: [완료 조건]
```

---

## 태스크 분해 원칙

### 태스크 크기 기준
- 1 태스크 = 1개 파일 or 1개 함수 단위가 이상적
- 컨텍스트 윈도우 70% 이내에서 완료 가능한 크기
- 태스크 간 의존성이 있으면 선행 태스크 완료 + 컨펌 후 다음 지시

### 태스크 연결 규칙
```
TASK-001 완료 → 붐 검토 → Human 컨펌 → TASK-002 지시
```
- 컨펌 없이 다음 태스크 시작 금지
- 이전 태스크 결과물을 다음 태스크 지시에 컨텍스트로 포함

### 태스크 ID 체계
```
TASK-[스프린트번호]-[순번]  예: TASK-01-003
```

---

## 업무 흐름 (Sprint 단위)

```
[Sprint 시작]
1. Human → 붐 (한국어): 이번 스프린트 목표 전달
2. 붐: CLAUDE.md 기반 태스크 분해 → Human에게 목록 보고 (한국어)
3. Human: 태스크 목록 컨펌 ✅

[태스크 실행 루프 — 태스크 하나씩 반복]
4. 붐 → DeepSeek/Claude Code (영어): 태스크 지시 (표준 양식)
5. DeepSeek/Claude Code: 구현 + 단위 테스트 생성 → 붐에게 보고
6. 붐: 결과물 1차 검토 (레이어 경계 / 네이밍 / 하드코딩 여부)
7. 붐 → GPT-4o (영어): 코드 품질 리뷰 지시
8. 붐 → Gemini (영어): 테스트 시나리오 작성 지시
9. GPT-4o / Gemini: QA 결과 보고
10. 붐: QA 결과 종합 + 설계 정합성 검증 → Human에게 보고 (한국어)
11. Human: 컨펌 ✅ or 수정 지시
12. Human: 빌드 검증 + 커밋
13. 붐: 대시보드 상태 업데이트 (VSE_Sprint_Status.json)
→ 다음 태스크로

[Sprint 종료]
14. 붐 → Human: 스프린트 회고 보고 (한국어)
    - 완료 태스크 / 미완료 태스크
    - 설계 오류 / 누락 발견 사항
    - 다음 스프린트 제안
```

---

## 단계별 컨펌 기준

| 단계 | 컨펌 필요 여부 | 컨펌 내용 |
|---|---|---|
| 스프린트 태스크 목록 | ✅ 필수 | 목록 확인 + 우선순위 조정 |
| 태스크 완료 후 QA 전 | ✅ 필수 | 구현 결과물 확인 |
| QA 완료 후 커밋 전 | ✅ 필수 | 최종 확인 |
| 설계 오류/누락 발견 시 | ✅ 즉시 에스컬레이션 | 구현 중단 + Human 판단 요청 |
| 단순 수정 (오타, 주석) | ❌ 불필요 | 붐 자체 처리 가능 |

---

## 설계 정합성 검증 체크리스트

붐은 각 태스크 완료 시 아래 항목을 체크하고 Human에게 보고한다.

```
[ ] Layer 경계 위반 없음 (Layer 0에 구현체 없음)
[ ] 하드코딩 없음 (수치 전부 ConfigManager/JSON)
[ ] 네이밍 컨벤션 준수 (CLAUDE.md 기준)
[ ] 메모리 소유권 컨벤션 준수 (unique_ptr 우선)
[ ] 에러 핸들링 정책 준수 (exception 없음)
[ ] Phase 1 범위 초과 없음
[ ] 단위 테스트 포함됨
[ ] CLAUDE.md와 구현 내용 일치
```

---

## 설계 오류 / 누락 발견 시 처리 절차

개발 도중 설계 오류나 CLAUDE.md에 없는 케이스가 발견되면:

```
1. 붐: 구현 즉시 중단
2. 붐 → Human 에스컬레이션 (한국어):
   - 발견 위치 (파일명 / 태스크 ID)
   - 문제 내용
   - 예상 영향 범위
   - 붐의 해결 방안 제안 (선택지 2~3개)
3. Human: 방향 결정
4. 필요 시 CLAUDE.md 업데이트 (Human 승인 후)
5. 업데이트된 CLAUDE.md 기준으로 구현 재개
```

**에스컬레이션 우선순위:**
- 🔴 즉시: Layer 경계 파괴, Phase 범위 초과, 아키텍처 충돌
- 🟡 태스크 완료 후: 네이밍 불일치, 누락 예외 처리
- 🟢 스프린트 회고 시: 개선 제안, 효율화 아이디어

---

## 모델별 역할 & 절대 규칙

### 붐 (Boom) — PM

**절대 규칙:**
- Human과 한국어로 대화, 개발팀에는 영어로 지시
- CLAUDE.md에 없는 결정을 임의로 내리지 않는다
- 태스크 컨펌 없이 다음 단계 진행 금지
- Human 확인 없이 Phase 범위 확장 금지
- Opus → Sonnet 전환 후 첫 작업은 CLAUDE.md 재독 필수
- 설계 오류 발견 즉시 에스컬레이션

---

### DeepSeek — 개발

**절대 규칙:**
- Layer 경계 위반 금지 (Layer 0 인터페이스 직접 구현 금지)
- CLAUDE.md 네이밍 컨벤션 준수
- 하드코딩 금지 — 수치는 ConfigManager/JSON
- 단위 테스트 반드시 함께 생성
- Phase 1 범위 밖 선제 구현 금지
- 메모리 소유권 컨벤션 준수 (unique_ptr 우선)
- 태스크 완료 시 붐에게 보고: 구현 파일명 + 변경 내용 + 불확실한 점

---

### Claude Code (Sonnet) — 렌더링

**절대 규칙:**
- Layer 3만 담당 — 게임 로직 침범 금지
- Dear ImGui는 디버그 용도만 (Phase 1)
- IRenderCommand 인터페이스를 통해서만 게임 상태 접근
- 태스크 완료 시 붐에게 보고: 렌더링 성능 수치 포함

---

### GPT-4o — QA 코드 품질

**절대 규칙:**
- 리뷰 결과: Pass / Fail / Warning 3단계 명시
- Fail은 파일명 + 라인 수준으로 지적
- 아키텍처 변경 제안 금지 — 붐을 통해서만
- CLAUDE.md 기준으로만 판단

---

### Gemini 2.5 Flash — QA 통합 테스트

**절대 규칙:**
- 테스트 시나리오: 입력값 + 기대값 명시 (재현 가능)
- 코드 수정 제안 금지 — 버그 리포트만
- Phase 1 범위 밖 테스트 작성 금지
- 성능 기준: 60fps / NPC 50명 기준

---

## 모델 전환 기준 (붐)

| 상황 | 모델 |
|---|---|
| 일반 PM, 지시 조율, 진행 상황 확인 | Sonnet |
| 아키텍처 결정, 설계 변경 | Opus |
| CLAUDE.md 내용 변경 검토 | Opus |
| 복잡한 버그 원인 분석 | Opus |
| 설계 오류 에스컬레이션 판단 | Opus |

---

## 공통 금지 사항 (전 모델)

- Phase 1 범위 밖 선제 구현 금지
- CLAUDE.md 미확인 상태에서 구현 시작 금지
- 하드코딩 금지
- Layer 경계 위반 금지
- Human 확인 없이 외부 라이브러리 추가 금지
- AI 간 직접 지시 금지 — 반드시 붐(PM)을 통할 것
- QA 결과 없이 커밋 금지

---

## 대시보드 연동 (VSE_Sprint_Status.json)

붐은 각 태스크 완료 / 컨펌 후 아래 파일을 업데이트한다.
대시보드는 이 파일을 읽어 실시간 렌더링.

**파일 위치:** `VSE_Sprint_Status.json`

```json
{
  "sprint": 1,
  "updated_at": "2026-03-24T00:00:00",
  "phase": "Phase 1",
  "sprint_goal": "스프린트 목표",
  "overall_status": "in_progress",
  "tasks": [
    {
      "id": "TASK-01-001",
      "title": "태스크 제목",
      "assignee": "DeepSeek",
      "layer": "Layer 1",
      "status": "done",
      "qa_status": "pass",
      "design_check": "pass",
      "confirmed_by_human": true,
      "notes": ""
    }
  ],
  "escalations": [],
  "design_issues": []
}
```

**status 값:** `pending` / `in_progress` / `done` / `blocked`
**qa_status 값:** `pending` / `pass` / `fail` / `warning`
**design_check 값:** `pending` / `pass` / `issue`

---

## 토큰 한도 / 컨텍스트 중단 복구 절차

### 중단 감지 기준
- 모델이 응답 중 멈추거나 잘림
- "I've reached my context limit" 류 메시지
- 응답이 갑자기 불완전하게 끊김

### 복구 절차 (붐 기준)

```
[중단 발생]
1. 붐: 현재 태스크 상태를 VSE_Sprint_Status.json에 즉시 저장
   - status: "in_progress" 유지
   - notes 필드에 "INTERRUPTED at: [마지막 완료 지점]" 기록

2. 새 세션 시작 시 붐이 Human에게 보고:
   "이전 세션이 [TASK-XX-XXX] 도중 중단되었습니다.
    마지막 완료 지점: [내용]
    재개 방법: [선택지]"

3. Human 컨펌 후 재개
```

### 태스크 중단 방지 설계
- 1 태스크 = 1 컨텍스트 내에서 완료 가능한 크기 (엄수)
- 태스크 시작 전 붐이 크기 예측 — 위험 시 더 작게 분해
- 태스크 완료마다 git commit (아래 참조)
- 중단되어도 커밋된 것은 손실 없음

### 새 세션 재개 시 붐의 필수 첫 행동
```
1. CLAUDE.md 읽기
2. VSE_Sprint_Status.json 읽기
3. 마지막 git log 확인
4. Human에게 현재 상태 요약 보고 (한국어)
5. Human 컨펌 후 다음 태스크 진행
```

---

## Git 버전 관리 규칙

### 커밋 타이밍 (필수)
| 시점 | 커밋 여부 |
|---|---|
| 태스크 완료 + Human 컨펌 | ✅ 필수 커밋 |
| QA Pass | ✅ 필수 커밋 |
| CLAUDE.md 변경 | ✅ 필수 커밋 |
| 스프린트 종료 | ✅ 필수 커밋 + 태그 |
| 작업 중간 저장 | ✅ WIP 커밋 허용 |

### 커밋 메시지 형식
```
[TASK-01-003] feat: implement GridSystem tile coordinate

- Added TileCoordinate struct with x, y, floor fields
- Configured origin at bottom-left per CLAUDE.md
- Unit tests included: GridSystemTest.cpp

Reviewed-by: GPT-4o (pass)
Confirmed-by: Human
```

**타입 prefix:**
- `feat:` 새 기능
- `fix:` 버그 수정
- `refactor:` 구조 개선 (기능 변경 없음)
- `test:` 테스트 추가
- `docs:` 문서 변경
- `wip:` 작업 중 임시 저장

### 브랜치 전략
```
main          — 항상 빌드 가능. Human 컨펌된 것만.
dev           — 스프린트 작업 브랜치
feature/XXX   — 태스크별 브랜치 (선택)
```

### 태그 규칙
```
sprint-01-end   — 스프린트 종료 시
phase1-mvp      — Phase 1 완료 시
v0.1.0          — Steam 출시 전 빌드
```

---

## 코드 주석 규칙 (전 모델 필수)

### 원칙
- **모든 코드에 주석 필수** — 다른 모델이 인수인계 받아도 즉시 이해 가능해야 함
- 영어 주석 허용 (모델 간 호환성)
- "무엇을 하는지"보다 **"왜 이렇게 했는지"** 중심으로 작성

### 필수 주석 위치

**파일 상단 (모든 파일):**
```cpp
/**
 * @file GridSystem.cpp
 * @layer Layer 1 - Domain Module
 * @task TASK-01-001
 * @author DeepSeek
 * @reviewed GPT-4o (pass, 2026-03-24)
 *
 * @brief Implements IGridSystem for the RealEstate domain.
 *        Manages tile coordinates for a vertical building grid (max 30 floors).
 *
 * @note Phase 1 only: vertical grid. Horizontal support planned for Phase 2
 *       (Apartment Manager domain). Do NOT modify IGridSystem interface
 *       without Human approval.
 *
 * @see CLAUDE.md > Layer 0 Module List
 */
```

**클래스/함수 단위:**
```cpp
/**
 * @brief Converts world position to tile coordinate.
 *
 * Origin is bottom-left (0,0). Y increases upward.
 * This convention matches SimTower's coordinate system.
 *
 * @param worldX  Pixel X position
 * @param worldY  Pixel Y position
 * @return TileCoordinate {x, y, floor}
 */
TileCoordinate worldToTile(int worldX, int worldY);
```

**Phase 슬롯 주석 (Phase 2/3 예정 코드):**
```cpp
// [PHASE-2] IIoTAdapter slot: BACnet/IP integration planned here.
// Thread isolation + async queue pattern will be used.
// See VSE_AI_Team.md > Phase 2 TODO for details.
```

**설계 결정 근거 주석:**
```cpp
// DESIGN DECISION: Using LOOK algorithm instead of Destination Dispatch.
// Rationale: MVP scope (2-4 elevators, 30 floors max).
// Destination Dispatch will be added in v1.1.
// See: VSE_Architecture_Review.md > Confirmed Decisions #5
```

### 인수인계 체크리스트 (태스크 완료 시)
```
[ ] 파일 상단 헤더 주석 포함 (@file, @layer, @task, @author, @brief)
[ ] 모든 public 함수에 @brief 주석
[ ] Phase 슬롯은 [PHASE-X] 태그로 표시
[ ] 설계 결정 근거 주석 포함 (CLAUDE.md 참조)
[ ] 다른 모델이 보고 즉시 이해 가능한지 자체 확인
```

---

## 변경 이력

| 날짜 | 버전 | 내용 |
|---|---|---|
| 2026-03 | 1.0 | 최초 작성. |
| 2026-03 | 2.0 | 언어 규칙, 태스크 분해, 단계별 컨펌, 설계 정합성 검증, 에스컬레이션, 대시보드 연동 추가. |
| 2026-03 | 2.1 | 토큰 중단 복구, git 버전 관리, 코드 주석 규칙, 인수인계 체크리스트 추가. |
