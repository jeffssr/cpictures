# 长图/宽图浏览 Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** 修复长图/宽图 100% 显示裁切后不可浏览、切换报错的问题，并加入 Shift 手型拖动和 ↑/↓ 滚动。

**Architecture:** 纯几何函数负责计算 100% 视口尺寸和 pan clamp，单元测试覆盖。`ViewState` 保存 pan offset，`D2DRenderer` 根据 pan 绘制。`ViewerWindow` 处理 Shift 拖动、键盘滚动、切图重置。

**Tech Stack:** C++20、Win32、Direct2D、CMake、CTest。

## Global Constraints

- 竖长图：左右 100%，上下最多屏幕高度。
- 横宽图：上下 100%，左右最多屏幕宽度。
- 按住 `Shift` 才切换手型并允许左键拖动内容。
- `↑` / `↓` 滚动内容：竖长图上下，横宽图左右。
- 鼠标滚轮继续前后切图。
- 不添加滚动条、工具栏、状态栏。

---

### Task 1: 几何函数和 pan clamp

**Files:**
- Modify: `include/cpictures/geometry.h`
- Modify: `tests/core_tests.cpp`

**Interfaces:**
- Produces: `SizeI ActualSizeViewport(SizeI image, SizeI workArea);`
- Produces: `PointI ClampPanOffset(PointI pan, SizeI content, SizeI viewport);`
- Produces: `bool CanPan(SizeI content, SizeI viewport);`

- [ ] **Step 1: Write failing tests**

Add to `TestFitWindow()` in `tests/core_tests.cpp`:

```cpp
    const cpictures::SizeI tall{800, 4000};
    const cpictures::SizeI tallViewport = cpictures::ActualSizeViewport(tall, work);
    Expect(tallViewport.width == 800, "tall image keeps 100 percent width");
    Expect(tallViewport.height == 1080, "tall image caps viewport height");

    const cpictures::SizeI wide{4000, 800};
    const cpictures::SizeI wideViewport = cpictures::ActualSizeViewport(wide, work);
    Expect(wideViewport.width == 1920, "wide image caps viewport width");
    Expect(wideViewport.height == 800, "wide image keeps 100 percent height");

    const cpictures::SizeI hugeViewport = cpictures::ActualSizeViewport(image, work);
    Expect(hugeViewport.width == 1920, "huge image caps viewport width");
    Expect(hugeViewport.height == 1080, "huge image caps viewport height");

    const cpictures::PointI clamped = cpictures::ClampPanOffset({900, -20}, {4000, 3000}, work);
    Expect(clamped.x == 900, "pan x inside range");
    Expect(clamped.y == 0, "pan y clamps to zero");

    const cpictures::PointI maxPan = cpictures::ClampPanOffset({9999, 9999}, {4000, 3000}, work);
    Expect(maxPan.x == 2080, "pan x clamps to max");
    Expect(maxPan.y == 1920, "pan y clamps to max");
```

- [ ] **Step 2: Verify RED**

Run:

```powershell
& 'C:\Program Files\Microsoft Visual Studio\18\Community\Common7\Tools\Launch-VsDevShell.ps1' -Arch amd64 -HostArch amd64 | Out-Null
cmake --build --preset debug --target cpictures_core_tests -- /m:1
```

Expected: compile fails because `ActualSizeViewport` and `ClampPanOffset` are missing.

- [ ] **Step 3: Implement minimal geometry**

Add inline functions to `include/cpictures/geometry.h`:

```cpp
inline SizeI ActualSizeViewport(SizeI image, SizeI workArea) {
    if (!IsValid(image) || !IsValid(workArea)) {
        return {};
    }
    return {std::min(image.width, workArea.width), std::min(image.height, workArea.height)};
}

inline bool CanPan(SizeI content, SizeI viewport) {
    return IsValid(content) && IsValid(viewport) &&
           (content.width > viewport.width || content.height > viewport.height);
}

inline PointI ClampPanOffset(PointI pan, SizeI content, SizeI viewport) {
    const int maxX = std::max(0, content.width - viewport.width);
    const int maxY = std::max(0, content.height - viewport.height);
    return {std::clamp(pan.x, 0, maxX), std::clamp(pan.y, 0, maxY)};
}
```

- [ ] **Step 4: Verify GREEN**

Run:

```powershell
cmake --build --preset debug --target cpictures_core_tests -- /m:1
ctest --preset debug -R cpictures_core_tests --output-on-failure
```

Expected: core tests pass.

- [ ] **Step 5: Commit**

```powershell
git add include/cpictures/geometry.h tests/core_tests.cpp
git commit -m "feat(viewer): add pan geometry"
```

---

### Task 2: Renderer pan offset

**Files:**
- Modify: `include/cpictures/view_state.h`
- Modify: `src/render/d2d_renderer.cpp`

**Interfaces:**
- Consumes: `ViewState::panX`, `ViewState::panY`.
- Produces: renderer draws oversized image from top/left pan offset while centering non-scrollable direction.

- [ ] **Step 1: Extend ViewState**

Add:

```cpp
    int panX = 0;
    int panY = 0;
```

- [ ] **Step 2: Apply pan in renderer**

Replace destination rect calculation with:

```cpp
        const float left = drawWidth > clientSize.width
            ? -static_cast<float>(state.panX)
            : (clientSize.width - drawWidth) * 0.5f;
        const float top = drawHeight > clientSize.height
            ? -static_cast<float>(state.panY)
            : (clientSize.height - drawHeight) * 0.5f;
        const D2D1_RECT_F destination = D2D1::RectF(left, top, left + drawWidth, top + drawHeight);
```

- [ ] **Step 3: Build**

Run:

```powershell
cmake --build --preset debug --target cpictures -- /m:1
```

Expected: `cpictures.exe` builds.

- [ ] **Step 4: Commit**

```powershell
git add include/cpictures/view_state.h src/render/d2d_renderer.cpp
git commit -m "feat(viewer): render panned images"
```

---

### Task 3: Viewer pan controls

**Files:**
- Modify: `src/viewer/viewer_window.h`
- Modify: `src/viewer/viewer_window.cpp`

**Interfaces:**
- Consumes: `ActualSizeViewport`, `ClampPanOffset`, `CanPan`.
- Produces: `ResetPan()`, `ClampCurrentPan()`, `PanBy(int dx, int dy)`, `CanPanCurrentImage()`.

- [ ] **Step 1: Update actual-size window sizing**

Use `ActualSizeViewport(decoded_.size, WorkAreaForWindow())` for actual-size load and zoom-1 view. This makes tall `800x4000` open as `800x1080`, wide `4000x800` open as `1920x800`.

- [ ] **Step 2: Reset and clamp pan**

Reset pan on:

```cpp
LoadCurrentImage(...)
ShowNextImage()
ShowPreviousImage()
SetActualSize()
SetFitToScreen()
```

Clamp pan after resize and after pan movement.

- [ ] **Step 3: Add Shift drag**

On `WM_LBUTTONDOWN`, if `Shift` is down and current image can pan:

```cpp
panningImage_ = true;
panDragStartPoint_ = mouse point;
panDragStartOffset_ = {viewState_.panX, viewState_.panY};
SetCapture(hwnd_);
SetCursor(LoadCursorW(nullptr, IDC_HAND));
return 0;
```

On `WM_MOUSEMOVE`, if `panningImage_`:

```cpp
PanTo({
    panDragStartOffset_.x - dx,
    panDragStartOffset_.y - dy
});
return 0;
```

On `WM_LBUTTONUP`, stop pan drag and release capture.

- [ ] **Step 4: Add cursor and key scroll**

On `WM_SETCURSOR`, if `Shift` down and can pan, set `IDC_HAND`.

On `WM_KEYDOWN`:

```cpp
if (wparam == VK_UP) {
    PanBy(0, -96) or PanBy(-96, 0) depending on dominant overflow;
}
if (wparam == VK_DOWN) {
    PanBy(0, 96) or PanBy(96, 0) depending on dominant overflow;
}
```

Dominant overflow: if vertical overflow exists, up/down pan Y; else horizontal pan X.

- [ ] **Step 5: Build**

Run:

```powershell
cmake --build --preset debug --target cpictures -- /m:1
```

Expected: `cpictures.exe` builds.

- [ ] **Step 6: Commit**

```powershell
git add src/viewer/viewer_window.h src/viewer/viewer_window.cpp
git commit -m "feat(viewer): add shift drag panning"
```

---

### Task 4: Manual repro and full verification

**Files:**
- No code edits expected.

**Interfaces:**
- Consumes all previous tasks.

- [ ] **Step 1: Build and test**

Run:

```powershell
& 'C:\Program Files\Microsoft Visual Studio\18\Community\Common7\Tools\Launch-VsDevShell.ps1' -Arch amd64 -HostArch amd64 | Out-Null
cmake --build --preset debug -- /m:1
ctest --preset debug --output-on-failure
cmake --build --preset release --target cpictures -- /m:1
```

Expected:

```text
100% tests passed, 0 tests failed out of 6
```

- [ ] **Step 2: Manual smoke**

Create temporary tall and wide BMPs, open with Debug `cpictures.exe`, verify:

- Tall opens with full width and capped height.
- Wide opens with full height and capped width.
- `Shift` changes cursor behavior to image pan.
- `↑` / `↓` pan content.
- Switching tall <-> normal image does not show `startup failed`.

- [ ] **Step 3: Clean status**

Run:

```powershell
git diff --check
git status --short --branch
```

Expected: no whitespace errors, clean worktree.

---

## Self-Review

- Spec coverage: viewport sizing, pan state, Shift hand drag, ↑/↓ pan, wheel unchanged, no visible UI all covered.
- Placeholder scan: no `TBD`, no `TODO`, no “implement later”.
- Type consistency: `ActualSizeViewport`, `ClampPanOffset`, `CanPan`, `panX`, `panY` used consistently.

