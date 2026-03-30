# VSE Platform Sprint 6 — 붐4 빌드+테스트 QA 계획서

> 작성: 붐 (claude-sonnet-4-6) | 2026-03-28
> 실행: 붐4 (qwen3-coder:30b-a3b-q4_K_M)
> 환경: 맥미니 M4 Pro, macOS, cmake 4.3.0, clang 21.0.0

---

## 목표
Sprint 6에서 추가된 렌더러/오디오 시스템을 포함한 전체 프로젝트를:
1. 실제로 빌드하고
2. 기존 자동화 테스트(382개) 전부 통과하는지 확인하고
3. Sprint 6 신규 코드 전용 추가 테스트를 실행해
4. 최종 QA 보고서를 작성한다.

---

## Phase 1 — 클린 빌드

### 목적
기존 build-mac 캐시를 버리고 완전 새로 빌드해서 숨겨진 빌드 에러/경고를 잡는다.

### 스크립트
```bash
#!/bin/bash
# qa_build_sprint6.sh — 붐4 실행용

set -e
PROJECT="/Users/sykim/workspace/vse-platform"
BUILD_DIR="$PROJECT/build-mac"
REPORT="$PROJECT/reports/QA_Sprint6_Build_Boom4.md"

echo "# VSE Sprint 6 Build QA Report" > "$REPORT"
echo "**Date:** $(date '+%Y-%m-%d %H:%M KST')" >> "$REPORT"
echo "**Reviewer:** Boom4 (qwen3-coder)" >> "$REPORT"
echo "" >> "$REPORT"

# Step 1: 클린 빌드
echo "## Phase 1: Clean Build" >> "$REPORT"
echo "\`\`\`" >> "$REPORT"

cd "$BUILD_DIR"
cmake --build . --parallel 4 --clean-first 2>&1 | tee -a "$REPORT"

echo "\`\`\`" >> "$REPORT"

# 빌드 결과 판정
if [ $? -eq 0 ]; then
    echo "**Build Result: ✅ PASS**" >> "$REPORT"
else
    echo "**Build Result: ❌ FAIL**" >> "$REPORT"
    echo "CRITICAL: Build failed — stopping QA" >> "$REPORT"
    exit 1
fi
```

### 확인 항목
- [ ] 빌드 에러 0개
- [ ] 경고(warning) 목록 수집 (WARNING 이상만)
- [ ] TowerTycoon 실행 파일 생성 확인: `build-mac/TowerTycoon`

---

## Phase 2 — 전체 자동화 테스트 (ctest 382개)

### 목적
기존 382개 테스트가 Sprint 6 코드 추가 후에도 전부 통과하는지 확인한다.

### 스크립트
```bash
# Phase 2: ctest 전체 실행
echo "## Phase 2: Full Test Suite (382 tests)" >> "$REPORT"
echo "\`\`\`" >> "$REPORT"

cd "$BUILD_DIR"
ctest --output-on-failure --parallel 4 2>&1 | tee -a "$REPORT"

PASS=$(ctest --output-on-failure 2>&1 | grep -c "Passed")
FAIL=$(ctest --output-on-failure 2>&1 | grep -c "Failed")

echo "\`\`\`" >> "$REPORT"
echo "" >> "$REPORT"
echo "**Tests Passed:** $PASS" >> "$REPORT"
echo "**Tests Failed:** $FAIL" >> "$REPORT"

if [ "$FAIL" -eq 0 ]; then
    echo "**Test Result: ✅ PASS (all $PASS passed)**" >> "$REPORT"
else
    echo "**Test Result: ❌ FAIL ($FAIL failed)**" >> "$REPORT"
fi
```

### 확인 항목
- [ ] 382/382 통과
- [ ] 실패 테스트명 목록화
- [ ] 실패 원인 분석 (Sprint 6 변경과 연관성 확인)

---

## Phase 3 — Sprint 6 신규 코드 커버리지 분석

### 목적
Sprint 6에서 추가된 파일들이 기존 테스트로 커버되는지 확인한다.

### Sprint 6 신규 파일 목록
| 파일 | 관련 기존 테스트 | 커버 여부 |
|------|----------------|----------|
| src/renderer/TileRenderer.cpp | test_RenderFrame.cpp | 부분 |
| src/renderer/ElevatorRenderer.cpp | test_RenderAgent.cpp | 부분 |
| src/renderer/SpriteSheet.cpp | test_SpriteSheet.cpp | ✅ |
| src/renderer/AudioEngine.cpp | **없음** | ❌ |
| src/renderer/DayNightCycle.cpp | **없음** | ❌ |
| src/renderer/SDLRenderer.cpp (facade) | test_RenderFrame.cpp | 부분 |

### 스크립트
```bash
# Phase 3: 커버리지 확인
echo "## Phase 3: Sprint 6 Coverage Analysis" >> "$REPORT"

for FILE in TileRenderer ElevatorRenderer SpriteSheet AudioEngine DayNightCycle; do
    COVERED=$(grep -r "$FILE" "$PROJECT/tests/" --include="*.cpp" -l 2>/dev/null | tr '\n' ' ')
    if [ -z "$COVERED" ]; then
        echo "- ❌ $FILE: NO test coverage" >> "$REPORT"
    else
        echo "- ✅ $FILE: covered by $COVERED" >> "$REPORT"
    fi
done
```

---

## Phase 4 — Sprint 6 신규 테스트 작성 및 실행

### 목적
AudioEngine, DayNightCycle 등 커버리지 없는 파일에 대한 경량 테스트를 작성하고 실행한다.

### 테스트 항목 — AudioEngine
```cpp
// tests/test_AudioEngine.cpp (붐4가 작성할 파일)
// 1. AudioEngine 기본 초기화 (SDL 없이 VSE_HAS_AUDIO=0 모드)
// 2. init() 호출 후 isInitialized() == true
// 3. shutdown() 이중 호출 안전성
// 4. playBGM("nonexistent.ogg") → crash 없이 false 반환
// 5. playSFX("nonexistent.wav") → crash 없이 false 반환
// 6. setVolume(-1) → clamp 처리 (0으로)
// 7. setVolume(200) → clamp 처리 (100으로)
```

### 테스트 항목 — DayNightCycle
```cpp
// tests/test_DayNightCycle.cpp (붐4가 작성할 파일)
// 1. hour=0 → 새벽 색상 (어두운 청색 계열)
// 2. hour=6 → 일출 색상 (주황 계열)
// 3. hour=12 → 낮 색상 (밝은 노란/흰 계열)
// 4. hour=18 → 일몰 색상 (주황/붉은 계열)
// 5. hour=22 → 야간 색상 (어두운 계열)
// 6. hour=24 → 범위 초과 처리 (crash 없음)
// 7. getOverlayAlpha() 반환값 0~255 범위 검증
```

### 스크립트
```bash
# Phase 4: 신규 테스트 빌드 및 실행
echo "## Phase 4: Sprint 6 New Tests" >> "$REPORT"

cd "$BUILD_DIR"
cmake --build . --parallel 4 2>&1 | grep -E "AudioEngine|DayNight|error:|warning:" | tee -a "$REPORT"
ctest -R "AudioEngine|DayNight" --output-on-failure 2>&1 | tee -a "$REPORT"
```

---

## Phase 5 — 최종 보고서 작성

### 보고서 형식
```
# Sprint 6 Full QA Report — Boom4
**Date:** YYYY-MM-DD
**Environment:** macOS M4 Pro, cmake 4.3.0, clang 21.0.0

## Build Summary
- Clean build: PASS/FAIL
- Warnings: N개
- Binary: build-mac/TowerTycoon (XX MB)

## Test Summary
| Suite | Total | Passed | Failed |
|-------|-------|--------|--------|
| Existing (Sprint 1-5) | 382 | ? | ? |
| Sprint 6 New | ? | ? | ? |
| **Total** | **?** | **?** | **?** |

## Issues Found
| Severity | File | Line | Description |
|----------|------|------|-------------|
| WARNING | ... | ... | ... |

## Sprint 6 Coverage
| File | Coverage |
|------|----------|
| TileRenderer | ... |
| ElevatorRenderer | ... |
| SpriteSheet | ✅ |
| AudioEngine | ... |
| DayNightCycle | ... |

## Overall Verdict
PASS / FAIL / PASS_WITH_WARNINGS

## Recommendations
- ...
```

---

## 붐4 실행 지시 요약

붐4가 이 계획서를 받으면 순서대로 실행:

1. `cmake --build /Users/sykim/workspace/vse-platform/build-mac --parallel 4 --clean-first 2>&1`
2. `cd /Users/sykim/workspace/vse-platform/build-mac && ctest --output-on-failure 2>&1`
3. Sprint 6 파일별 테스트 커버리지 분석
4. `tests/test_AudioEngine.cpp` 작성 (원본 수정 금지)
5. `tests/test_DayNightCycle.cpp` 작성 (원본 수정 금지)
6. CMakeLists.txt에 신규 테스트 추가 (tests/CMakeLists.txt 수정)
7. 재빌드 후 신규 테스트 실행
8. 최종 보고서 → `reports/QA_Sprint6_Full_Boom4.md`

---

## 예상 소요 시간
| Phase | 예상 시간 |
|-------|---------|
| Phase 1: 클린 빌드 | 3~5분 |
| Phase 2: ctest 382개 | 2~3분 |
| Phase 3: 커버리지 분석 | 1분 |
| Phase 4: 신규 테스트 작성+실행 | 10~15분 |
| Phase 5: 보고서 작성 | 3~5분 |
| **총합** | **약 20~30분** |
