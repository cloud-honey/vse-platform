# Sprint 5 계획서 — 기능 완성 (플레이어블 빌드)

> 목표: **마우스로 짓고, 카메라로 보고, 저장/불러오기 되는 게임**

## 전체 로드맵 (Sprint 4 완료 시점 확정)

| 단계 | 내용 | 예상 |
|------|------|------|
| **Sprint 5** | **기능 완성 (건설 UI, 카메라, Save/Load, HUD)** | **3~5일** |
| Sprint 6 | 비주얼 + 사운드 (스프라이트, 애니메이션, BGM/SFX, UI 스킨) | 1~2주 |
| Sprint 7 | Steam SDK, 빌드 파이프라인 | 3~5일 |
| Sprint 8 | QA + 밸런싱 (경제 수치, 난이도) | 1주 |
| Sprint 9 | 성능 최적화 (NPC 50명, 30층) | 3~5일 |
| Sprint 10 | 베타 테스트 + 버그 수정 | 1~2주 |
| 출시 | Steam Early Access | - |

## Sprint 5 태스크 (7개)

### TASK-05-000: Design Spec v2.5 동기화 ⭐
- Sprint 4 구현 반영 + Sprint 5~10 로드맵 섹션 추가
- 담당: 붐

### TASK-05-001: 마우스 건설 UI ⭐⭐⭐
- BuildCursor 완성:
  - 건설 모드 진입/퇴출 (HUD 버튼 또는 단축키)
  - 마우스 hover → 타일 하이라이트 (유효=초록, 무효=빨강)
  - 클릭 → BuildFloor / PlaceTenant 커맨드
  - 테넌트 타입 선택 (Office/Residential/Commercial)
  - 건설 비용 tooltip
  - 잔고 부족 시 빨간 깜빡임
- 담당: 붐2 → 붐

### TASK-05-002: Camera 줌/팬 ⭐⭐
- 마우스 휠 → 줌 인/아웃 (0.5x ~ 3.0x)
- 우클릭 드래그 → 팬
- WASD / 화살표 → 팬
- 줌 범위 제한 + 가장자리 스크롤
- 담당: 붐2

### TASK-05-003: Save/Load UI ⭐⭐
- 세이브 슬롯 목록 (ImGui 리스트)
- 메타데이터 표시 (날짜, 잔고, 별점, 플레이 시간)
- New Save / Overwrite 확인 다이얼로그
- 자동 세이브 (매 5분)
- 담당: 붐2

### TASK-05-004: HUD 고도화 ⭐⭐
- 건설 모드 툴바 (층 건설 / Office / Residential / Commercial / Elevator)
- 별점 시각화 (★☆)
- 일간/주간 수익 표시
- 알림 토스트 (자금 부족, 결산, 별점 변경)
- 배속 컨트롤 (1x / 2x / 4x)
- 담당: 붐2 → 붐

### TASK-05-005: TD-001 isAnchor 리팩토링 ⭐
- TileData.isAnchor 의미 분리 (isElevatorShaft 사용처 전환)
- GridSystem, EconomyEngine, test mock 업데이트
- 담당: 붐2

### TASK-05-006: 통합 + Sprint 5 빌드 ⭐⭐⭐
- 마우스 건설 → NPC 스폰 → 수익 → 세이브 → 로드 통합 테스트
- Debug/Release/Windows 빌드
- 3모델 교차 검증
- 담당: 붐

## 작업 순서
1. 05-000 (Design Spec) + 05-005 (TD-001) — 작고 독립적
2. 05-002 (Camera) — 독립적, 빠름
3. 05-001 (건설 UI) — Sprint 5 핵심
4. 05-004 (HUD) — 건설 UI 위에 쌓기
5. 05-003 (Save/Load UI)
6. 05-006 (통합 빌드)

## 성공 기준
- [ ] 마우스로 층 건설 + 테넌트 배치 가능
- [ ] 카메라 줌/팬으로 건물 탐색 가능
- [ ] 세이브/로드 UI로 게임 저장/불러오기 가능
- [ ] HUD에서 건설 도구 선택 + 재정 상태 확인 가능
- [ ] Windows 빌드 배포 가능
