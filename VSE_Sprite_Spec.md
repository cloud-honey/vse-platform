# Tower Tycoon — 픽셀아트 스프라이트 제작 스펙

> 작성: 붐 | 2026-03-27 | Sprint 6 준비

---

## 기본 방향

- **레퍼런스:** SimTower (1994), YootTower
- **뷰:** 정면 2D 단면도 (건물을 잘라서 보는 시점)
- **색감:** 밝고 따뜻한 톤, 베이지/회색 건물 벽, 아이보리 배경
- **타일 크기:** 32×32 픽셀
- **포맷:** PNG, 투명 배경

---

## 1. NPC 스프라이트 시트

**파일명:** `content/sprites/npc_sheet.png`  
**크기:** 128×32 픽셀 (16픽셀 × 8프레임 가로 나열)

| 프레임 | 내용 |
|---|---|
| 0 | 걷기1 (오른쪽) |
| 1 | 걷기2 (오른쪽) |
| 2 | 걷기3 (오른쪽) |
| 3 | 걷기4 (오른쪽) |
| 4 | 대기 (서있음) |
| 5 | 엘리베이터 탑승 |
| 6 | 휴식 |
| 7 | 계단 이동 |

**프롬프트:**
```
pixel art sprite sheet, SimTower style tiny office worker character,
8 frames horizontal, 16x32 pixels per frame, transparent background,
very small simple character similar to SimTower 1994 game,
walk cycle 4 frames, idle 1 frame, elevator riding 1 frame,
resting 1 frame, stair walking 1 frame,
limited color palette 8-16 colors, clean crisp pixels,
retro 1990s Windows game style, no antialiasing
```

---

## 2. 건물 타일셋 (각 32×32, 투명 배경)

### 사무실 — `content/sprites/office_floor.png`

```
pixel art 32x32 tile, SimTower 1994 style office room,
front cross-section view, beige/cream colored office interior,
small desk with computer monitor, window on right side showing sky,
warm indoor lighting, retro 90s game graphics,
transparent background, 16 color palette, no antialiasing
```

---

### 주거 — `content/sprites/residential_floor.png`

```
pixel art 32x32 tile, SimTower 1994 style residential unit,
front cross-section view, small apartment interior,
single bed or futon visible, small window, cozy warm colors,
retro 90s Japanese game graphics style,
transparent background, 16 color palette, no antialiasing
```

---

### 상업 — `content/sprites/commercial_floor.png`

```
pixel art 32x32 tile, SimTower 1994 style restaurant or shop,
front cross-section view, counter visible,
orange/warm signage colors like SimTower fast food or shop,
retro 90s game graphics style,
transparent background, 16 color palette, no antialiasing
```

---

### 엘리베이터 샤프트 — `content/sprites/elevator_shaft.png`

```
pixel art 32x32 tile, SimTower style elevator shaft,
dark gray vertical rails on sides, center open space,
building wall cross-section, retro 90s game style,
transparent background, 16 color palette
```

---

### 엘리베이터 칸 (닫힘) — `content/sprites/elevator_car.png`

```
pixel art 32x32 tile, SimTower style elevator car,
silver/gray metallic box with sliding doors closed,
retro 90s game style, transparent background, 16 color palette
```

---

### 엘리베이터 칸 (열림) — `content/sprites/elevator_open.png`

```
pixel art 32x32 tile, SimTower style elevator car,
silver/gray metallic box with sliding doors open,
retro 90s game style, transparent background, 16 color palette
```

---

### 로비 — `content/sprites/lobby_floor.png`

```
pixel art 32x32 tile, SimTower style lobby floor,
front cross-section, ground floor entrance,
marble-like floor pattern, reception desk hint,
bright open area, retro 90s game style,
transparent background, 16 color palette
```

---

### 건물 외벽 — `content/sprites/building_facade.png`

```
pixel art 32x32 tile, SimTower style building exterior wall,
cross-section side view, gray concrete with window,
retro 90s office building style,
transparent background, 16 color palette
```

---

## 3. UI 아이콘 (각 16×16, 투명 배경)

**파일 위치:** `content/icons/`

| 파일명 | 내용 | 프롬프트 |
|---|---|---|
| `floor_icon.png` | 층 건설 | `pixel art 16x16 icon, floor/building construction symbol, SimTower style, transparent background` |
| `office_icon.png` | 사무실 | `pixel art 16x16 icon, office desk symbol, SimTower style, transparent background` |
| `residential_icon.png` | 주거 | `pixel art 16x16 icon, small house or bed symbol, SimTower style, transparent background` |
| `commercial_icon.png` | 상업 | `pixel art 16x16 icon, shop/restaurant symbol, SimTower style, transparent background` |

---

## 제작 우선순위

1. ✅ **NPC 스프라이트 시트** — 가장 중요
2. **사무실 타일**
3. **주거 타일**
4. **상업 타일**
5. **엘리베이터** (샤프트 + 칸 닫힘 + 칸 열림)
6. **로비 + 건물 외벽**
7. **UI 아이콘 4종**

---

## 완성 후 파일 구조

```
content/
  sprites/
    npc_sheet.png           (128×32)
    office_floor.png        (32×32)
    residential_floor.png   (32×32)
    commercial_floor.png    (32×32)
    elevator_shaft.png      (32×32)
    elevator_car.png        (32×32)
    elevator_open.png       (32×32)
    lobby_floor.png         (32×32)
    building_facade.png     (32×32)
  icons/
    floor_icon.png          (16×16)
    office_icon.png         (16×16)
    residential_icon.png    (16×16)
    commercial_icon.png     (16×16)
```
