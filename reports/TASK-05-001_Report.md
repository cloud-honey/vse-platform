# TASK-05-001 완료 보고서 — BuildCursor 완성

> 작성: 붐 (PM) | 날짜: 2026-03-27 | Sprint 5

---

## 개요

| 항목 | 내용 |
|---|---|
| Task ID | TASK-05-001 |
| 제목 | 마우스 건설 UI (BuildCursor 완성) |
| 담당 | 붐2 (DeepSeek) |
| 소요 시간 | 13분 35초 |
| 테스트 | 334/334 ✅ |
| 커밋 | 4a7e6ff |

---

## 구현 내용

### 1. BuildModeState 확장 (`include/core/InputTypes.h`)
- `bool isValidPlacement = true` 추가 — false 시 빨강 하이라이트
- `int64_t previewCost = 0` 추가 — tooltip 비용 표시용 (Cents 단위)

### 2. Bootstrapper — hover tile 유효성 + 비용 계산 (`src/core/Bootstrapper.cpp`)
- `frame.mouseX/mouseY` + Camera → hover TileCoord 계산
- **BuildFloor**: `!grid_->isFloorBuilt(hoverFloor)` → isValidPlacement
- **PlaceTenant**: `isFloorBuilt && isTileEmpty` → isValidPlacement
- **previewCost**:
  - BuildFloor: `config_.getInt("economy.floorBuildCost", 10000)`
  - PlaceTenant: `content_.getBalanceData()` → tenantType별 buildCost

### 3. BuildCursor 개선 (`include/renderer/BuildCursor.h`, `src/renderer/BuildCursor.cpp`)
- `drawFloorHighlight` / `drawTenantHighlight`: isValid 파라미터 추가
  - 유효: 초록 (0, 180, 80, 120)
  - 무효: 빨강 (220, 50, 50, 120)
- `drawCostTooltip` 신규:
  - 유효 시: "₩{cost}" (천단위 구분)
  - 무효 시: "Cannot build here" (빨강 텍스트)
  - previewCost = 0이고 유효하면 tooltip 생략
- `drawTenantSelectPopup` 신규:
  - ImGui 모달 팝업 "Select Tenant Type"
  - Office ₩5,000 / Residential ₩3,000 / Commercial ₩8,000 버튼
  - Cancel 버튼으로 선택 없이 닫기
- `openTenantSelectPopup()` 신규: popupOpen_ 플래그 세트

### 4. InputMapper — T키 팝업 트리거 (`include/renderer/InputMapper.h`, `src/renderer/InputMapper.cpp`)
- `openTenantPopup_` 플래그 추가
- T키 동작 변경:
  - 비PlaceTenant 모드: PlaceTenant 모드 진입 (Office, type 0)
  - PlaceTenant 모드: `openTenantPopup_ = true` (팝업 오픈)
- `shouldOpenTenantPopup()` / `clearTenantPopupFlag()` 메서드 추가

### 5. SDLRenderer — 팝업 피드백 처리 (`include/renderer/SDLRenderer.h`, `src/renderer/SDLRenderer.cpp`)
- `shouldOpenTenantPopup_` / `pendingTenantSelection_` 멤버 추가
- render 루프: 팝업 오픈 → `buildCursor_.drawTenantSelectPopup()` 호출 → 선택 결과 저장
- `checkTenantSelection(int& outTenantType)` / `setShouldOpenTenantPopup(bool)` 접근자 추가
- Bootstrapper에서 결과 수신 → InputMapper tenantType 업데이트

### 6. 테스트 (`tests/test_BuildCursor.cpp` 신규)
6개 테스트:
1. BuildModeState 신규 필드 기본값 확인
2. isValidPlacement 필드 세트/읽기
3. previewCost 필드 세트/읽기
4. Copy constructor — 신규 필드 보존 확인
5. Assignment operator — 신규 필드 보존 확인
6. 다른 action별 기본 비용 구조 확인

---

## 변경 파일 목록

| 파일 | 변경 유형 |
|---|---|
| `include/core/InputTypes.h` | 수정 (BuildModeState 필드 추가) |
| `include/renderer/BuildCursor.h` | 수정 (메서드 추가) |
| `include/renderer/InputMapper.h` | 수정 (팝업 플래그) |
| `include/renderer/SDLRenderer.h` | 수정 (팝업 접근자) |
| `src/core/Bootstrapper.cpp` | 수정 (유효성·비용 계산) |
| `src/renderer/BuildCursor.cpp` | 수정 (색상 분기, tooltip, 팝업) |
| `src/renderer/InputMapper.cpp` | 수정 (T키 동작) |
| `src/renderer/SDLRenderer.cpp` | 수정 (팝업 처리) |
| `tests/test_BuildCursor.cpp` | 신규 |
| `tests/CMakeLists.txt` | 수정 (테스트 등록) |

---

## 테스트 결과

```
334/334 tests passed
Total Test time: 4.01 sec
```

---

## Layer 경계 준수

- BuildCursor는 Layer 3 (renderer/) — domain 헤더 미포함 ✅
- BuildModeState는 Layer 0 데이터 계약 — 로직 없음 ✅
- Bootstrapper가 도메인 쿼리 후 RenderFrame에 채워 넘김 ✅

---

## 교차 검증

교차 검증 예정 (3모델: GPT-5.4 / Gemini 3.1 Pro / DeepSeek R1)
