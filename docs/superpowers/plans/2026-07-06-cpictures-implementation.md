# cpictures Core Viewer Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** 构建 cpictures 核心查看器：极速启动、只显示图片本身、默认 100% 原图贴合窗口、右键承载功能、支持 WIC 核心格式，并预留扩展格式组件接口。

**Architecture:** 主程序使用 C++20 + Win32 消息循环 + WIC 解码 + Direct2D/DXGI 渲染。核心查看路径不加载扩展 DLL；扩展格式通过稳定 C ABI 插件按需加载。安装器后置，负责 Win11 默认应用注册，不使用静默抢占关联。

**Tech Stack:** C++20、Win32 API、COM、WIC、Direct2D、DXGI、CMake、CTest、Windows SDK、后期 WiX Toolset。

## Global Constraints

- 所有用户可见文案默认简体中文。
- 平台：Windows 11。
- 主程序：C++20 + Win32 + WIC + Direct2D/DXGI。
- 默认窗口：无标题栏、无边框、无工具条，只显示图片本身。
- 默认显示：100% 原图；窗口客户区贴合图片尺寸，最大不超过屏幕工作区。
- 关闭：`Esc`。
- 信息角标：默认隐藏；单击图片任意区域显示/隐藏；内容为 `文件名.格式  n/m`；黑色背景 34% 不透明度，白字 90%。
- 鼠标：滚轮切图，`Ctrl + 滚轮` 缩放，右键打开菜单。
- 右键菜单：向左旋转、向右旋转、复制、复制路径、实际大小、适应屏幕、全屏、安装/更新格式支持。
- 第一版不做：删除、编辑、图库管理、打印、文件移动、另存为、批处理、常驻工具栏、标题栏、状态栏、静默下载、静默更新。
- 扩展下载必须用户确认，并校验 SHA-256。
- 当前目录起始状态不是 git 仓库；第一任务初始化 git，便于按任务提交。
- 当前 PATH 未发现 `cmake`、`ninja`、`cl`、`msbuild`；执行前需安装 Visual Studio 2022 Build Tools（Desktop development with C++）、Windows SDK、CMake，或在 “Developer PowerShell for VS 2022” 中执行。

---

## 文件结构

创建以下结构：

```text
D:\Jimmy\ai_docs\cpictures\
  CMakeLists.txt
  CMakePresets.json
  README.md
  docs\
    superpowers\
      specs\2026-07-06-cpictures-design.md
      plans\2026-07-06-cpictures-implementation.md
  include\
    cpictures\
      commands.h
      geometry.h
      image_document.h
      image_list.h
      natural_sort.h
      overlay.h
      supported_formats.h
      view_state.h
  src\
    app\
      app.cpp
      app.h
    codecs\
      codec_manager.cpp
      codec_manager.h
      cpictures_codec.h
    image\
      image_document.cpp
      image_list.cpp
      supported_formats.cpp
      wic_decoder.cpp
      wic_decoder.h
    platform\
      clipboard_service.cpp
      clipboard_service.h
      context_menu.cpp
      context_menu.h
      path_utils.cpp
      path_utils.h
      window_metrics.cpp
      window_metrics.h
    render\
      d2d_renderer.cpp
      d2d_renderer.h
    update\
      update_manifest.cpp
      update_manifest.h
      update_service.cpp
      update_service.h
    viewer\
      viewer_window.cpp
      viewer_window.h
    main.cpp
  tests\
    core_tests.cpp
    wic_tests.cpp
    assets\
      tiny.bmp
      tiny.png
  installer\
    wix\
      cpictures.wxs
```

责任边界：

- `include/cpictures/*`：纯逻辑类型和接口，尽量不引入 Win32 头，便于 CTest。
- `src/platform/*`：Win32/Shell/Clipboard/DPI/路径工具。
- `src/image/*`：目录扫描、格式判断、WIC 解码、图片文档。
- `src/render/*`：Direct2D 渲染和变换。
- `src/viewer/*`：窗口消息、输入、右键命令、窗口尺寸。
- `src/codecs/*`：扩展插件 ABI 和加载管理。
- `src/update/*`：扩展组件 manifest、hash、下载安装。
- `installer/wix/*`：Win11 默认应用和文件关联注册。

## 里程碑

1. 基础工程和可测试纯逻辑。
2. 可运行核心查看器：打开图片、100% 贴合、无 chrome 窗口、`Esc` 关闭。
3. 浏览和交互：同目录切图、缩放、全屏、角标、右键菜单、复制。
4. 扩展解码插件接口和格式支持入口。
5. 安装包和 Win11 默认应用注册第一阶段实现。

## Task 1: 初始化工程、构建和测试框架

**Files:**
- Create: `CMakeLists.txt`
- Create: `CMakePresets.json`
- Create: `README.md`
- Create: `tests/core_tests.cpp`

**Interfaces:**
- Produces: CMake target `cpictures_core_tests`。
- Produces: 测试入口 `int main()`，后续任务把纯逻辑测试追加到 `tests/core_tests.cpp`。

- [ ] **Step 1: 初始化 git 仓库**

Run:

```powershell
git init
git add docs/superpowers/specs/2026-07-06-cpictures-design.md docs/superpowers/plans/2026-07-06-cpictures-implementation.md
git commit -m "docs: add cpictures design and plan"
```

Expected:

```text
Initialized empty Git repository
[main (root-commit) ...] docs: add cpictures design and plan
```

- [ ] **Step 2: 写 CMake 根配置**

Create `CMakeLists.txt`:

```cmake
cmake_minimum_required(VERSION 3.24)
project(cpictures VERSION 0.1.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

option(CPICTURES_BUILD_TESTS "Build cpictures tests" ON)

include(CTest)
if(BUILD_TESTING AND CPICTURES_BUILD_TESTS)
  add_executable(cpictures_core_tests tests/core_tests.cpp)
  target_compile_features(cpictures_core_tests PRIVATE cxx_std_20)
  add_test(NAME cpictures_core_tests COMMAND cpictures_core_tests)
endif()
```

- [ ] **Step 3: 写 CMake presets**

Create `CMakePresets.json`:

```json
{
  "version": 6,
  "configurePresets": [
    {
      "name": "vs2022-x64",
      "displayName": "Visual Studio 2022 x64",
      "generator": "Visual Studio 17 2022",
      "architecture": "x64",
      "binaryDir": "${sourceDir}/build/vs2022-x64",
      "cacheVariables": {
        "CMAKE_INSTALL_PREFIX": "${sourceDir}/dist"
      }
    }
  ],
  "buildPresets": [
    {
      "name": "debug",
      "configurePreset": "vs2022-x64",
      "configuration": "Debug"
    },
    {
      "name": "release",
      "configurePreset": "vs2022-x64",
      "configuration": "Release"
    }
  ],
  "testPresets": [
    {
      "name": "debug",
      "configurePreset": "vs2022-x64",
      "configuration": "Debug",
      "output": {
        "outputOnFailure": true
      }
    }
  ]
}
```

- [ ] **Step 4: 写 README**

Create `README.md`:

```markdown
# cpictures

Win11 原生图片查看器。目标：极速、轻量、极简。

## 构建前提

- Visual Studio 2022 Build Tools，包含 Desktop development with C++
- Windows SDK
- CMake 3.24+

若普通 PowerShell 找不到 `cl` 或 `cmake`，使用 “Developer PowerShell for VS 2022”。

## 构建

```powershell
cmake --preset vs2022-x64
cmake --build --preset debug
ctest --preset debug
```

## 运行

```powershell
.\build\vs2022-x64\Debug\cpictures.exe C:\path\to\image.jpg
```
```

- [ ] **Step 5: 写空测试入口**

Create `tests/core_tests.cpp`:

```cpp
#include <cstdlib>
#include <iostream>

namespace {
void Expect(bool condition, const char* message) {
    if (!condition) {
        std::cerr << "FAIL: " << message << "\n";
        std::exit(1);
    }
}
}

int main() {
    Expect(true, "test harness runs");
    std::cout << "cpictures core tests passed\n";
    return 0;
}
```

- [ ] **Step 6: 验证配置、构建、测试**

Run:

```powershell
cmake --preset vs2022-x64
cmake --build --preset debug
ctest --preset debug
```

Expected:

```text
100% tests passed
```

- [ ] **Step 7: 提交**

Run:

```powershell
git add CMakeLists.txt CMakePresets.json README.md tests/core_tests.cpp
git commit -m "chore: add CMake project skeleton"
```

## Task 2: 纯逻辑核心类型、自然排序、窗口贴合计算

**Files:**
- Create: `include/cpictures/geometry.h`
- Create: `include/cpictures/natural_sort.h`
- Create: `include/cpictures/overlay.h`
- Create: `include/cpictures/supported_formats.h`
- Create: `include/cpictures/view_state.h`
- Create: `src/image/supported_formats.cpp`
- Create: `src/platform/window_metrics.cpp`
- Create: `src/platform/path_utils.cpp`
- Modify: `CMakeLists.txt`
- Modify: `tests/core_tests.cpp`

**Interfaces:**
- Produces: `cpictures::SizeI`、`RectI`、`FitImageWindow`。
- Produces: `cpictures::NaturalLess(std::wstring_view, std::wstring_view) -> bool`。
- Produces: `cpictures::BuildOverlayText(std::wstring_view fileName, int index, int count) -> std::wstring`。
- Produces: `cpictures::IsCoreSupportedExtension(std::wstring_view ext) -> bool`。

- [ ] **Step 1: 写失败测试**

Modify `tests/core_tests.cpp` to:

```cpp
#include <cstdlib>
#include <iostream>
#include <algorithm>
#include <string>
#include <vector>

#include "cpictures/geometry.h"
#include "cpictures/natural_sort.h"
#include "cpictures/overlay.h"
#include "cpictures/supported_formats.h"

namespace {
void Expect(bool condition, const char* message) {
    if (!condition) {
        std::cerr << "FAIL: " << message << "\n";
        std::exit(1);
    }
}

void TestNaturalSort() {
    std::vector<std::wstring> names{L"10.jpg", L"2.jpg", L"1.jpg"};
    std::sort(names.begin(), names.end(), cpictures::NaturalLess);
    Expect(names[0] == L"1.jpg", "natural sort first item");
    Expect(names[1] == L"2.jpg", "natural sort second item");
    Expect(names[2] == L"10.jpg", "natural sort third item");
}

void TestOverlayText() {
    const std::wstring text = cpictures::BuildOverlayText(L"DSC_1042.NEF", 16, 128);
    Expect(text == L"DSC_1042.NEF  17/128", "overlay text uses one-based index");
}

void TestSupportedFormats() {
    Expect(cpictures::IsCoreSupportedExtension(L".jpg"), "jpg supported");
    Expect(cpictures::IsCoreSupportedExtension(L".JPEG"), "jpeg case insensitive");
    Expect(!cpictures::IsCoreSupportedExtension(L".psd"), "psd not core supported");
}

void TestFitWindow() {
    const cpictures::SizeI image{4000, 3000};
    const cpictures::SizeI work{1920, 1080};
    const cpictures::SizeI fitted = cpictures::FitImageWindow(image, work);
    Expect(fitted.width == 1440, "fit width keeps aspect ratio");
    Expect(fitted.height == 1080, "fit height reaches work area");

    const cpictures::SizeI small{640, 480};
    const cpictures::SizeI exact = cpictures::FitImageWindow(small, work);
    Expect(exact.width == 640, "small image remains 100 percent width");
    Expect(exact.height == 480, "small image remains 100 percent height");
}
}

int main() {
    TestNaturalSort();
    TestOverlayText();
    TestSupportedFormats();
    TestFitWindow();
    std::cout << "cpictures core tests passed\n";
    return 0;
}
```

- [ ] **Step 2: 运行测试并确认失败**

Run:

```powershell
cmake --build --preset debug
ctest --preset debug
```

Expected:

```text
error C2039: 'NaturalLess': is not a member of 'cpictures'
```

- [ ] **Step 3: 写几何和视图头文件**

Create `include/cpictures/geometry.h`:

```cpp
#pragma once

#include <algorithm>

namespace cpictures {

struct SizeI {
    int width = 0;
    int height = 0;
};

struct PointI {
    int x = 0;
    int y = 0;
};

struct RectI {
    int x = 0;
    int y = 0;
    int width = 0;
    int height = 0;
};

inline bool IsValid(SizeI size) {
    return size.width > 0 && size.height > 0;
}

inline SizeI FitImageWindow(SizeI image, SizeI workArea) {
    if (!IsValid(image) || !IsValid(workArea)) {
        return {};
    }
    if (image.width <= workArea.width && image.height <= workArea.height) {
        return image;
    }
    const double scaleX = static_cast<double>(workArea.width) / image.width;
    const double scaleY = static_cast<double>(workArea.height) / image.height;
    const double scale = std::min(scaleX, scaleY);
    return {
        std::max(1, static_cast<int>(image.width * scale + 0.5)),
        std::max(1, static_cast<int>(image.height * scale + 0.5))
    };
}

}  // namespace cpictures
```

Create `include/cpictures/view_state.h`:

```cpp
#pragma once

namespace cpictures {

enum class FitMode {
    ActualSize,
    FitToScreen
};

struct ViewState {
    double zoom = 1.0;
    int rotationDegrees = 0;
    bool overlayVisible = false;
    bool fullscreen = false;
    FitMode fitMode = FitMode::ActualSize;
};

}  // namespace cpictures
```

- [ ] **Step 4: 写自然排序**

Create `include/cpictures/natural_sort.h`:

```cpp
#pragma once

#include <cwctype>
#include <string_view>

namespace cpictures {

inline int CompareNatural(std::wstring_view left, std::wstring_view right) {
    size_t i = 0;
    size_t j = 0;
    while (i < left.size() && j < right.size()) {
        const wchar_t lc = left[i];
        const wchar_t rc = right[j];
        if (std::iswdigit(lc) && std::iswdigit(rc)) {
            unsigned long long ln = 0;
            unsigned long long rn = 0;
            while (i < left.size() && std::iswdigit(left[i])) {
                ln = ln * 10 + static_cast<unsigned long long>(left[i] - L'0');
                ++i;
            }
            while (j < right.size() && std::iswdigit(right[j])) {
                rn = rn * 10 + static_cast<unsigned long long>(right[j] - L'0');
                ++j;
            }
            if (ln != rn) {
                return ln < rn ? -1 : 1;
            }
            continue;
        }
        const wchar_t a = static_cast<wchar_t>(std::towlower(lc));
        const wchar_t b = static_cast<wchar_t>(std::towlower(rc));
        if (a != b) {
            return a < b ? -1 : 1;
        }
        ++i;
        ++j;
    }
    if (i == left.size() && j == right.size()) {
        return 0;
    }
    return i == left.size() ? -1 : 1;
}

inline bool NaturalLess(std::wstring_view left, std::wstring_view right) {
    return CompareNatural(left, right) < 0;
}

}  // namespace cpictures
```

- [ ] **Step 5: 写角标和格式判断**

Create `include/cpictures/overlay.h`:

```cpp
#pragma once

#include <string>
#include <string_view>

namespace cpictures {

inline std::wstring BuildOverlayText(std::wstring_view fileName, int zeroBasedIndex, int totalCount) {
    std::wstring text(fileName);
    text += L"  ";
    text += std::to_wstring(zeroBasedIndex + 1);
    text += L"/";
    text += std::to_wstring(totalCount);
    return text;
}

}  // namespace cpictures
```

Create `include/cpictures/supported_formats.h`:

```cpp
#pragma once

#include <string>
#include <string_view>
#include <vector>

namespace cpictures {

std::wstring NormalizeExtension(std::wstring_view ext);
bool IsCoreSupportedExtension(std::wstring_view ext);
bool IsExtensionCandidate(std::wstring_view ext);
const std::vector<std::wstring>& CoreSupportedExtensions();
const std::vector<std::wstring>& ExtensionSupportedCandidates();

}  // namespace cpictures
```

Create `src/image/supported_formats.cpp`:

```cpp
#include "cpictures/supported_formats.h"

#include <algorithm>
#include <cwctype>

namespace cpictures {

std::wstring NormalizeExtension(std::wstring_view ext) {
    std::wstring value(ext);
    if (!value.empty() && value.front() != L'.') {
        value.insert(value.begin(), L'.');
    }
    std::transform(value.begin(), value.end(), value.begin(), [](wchar_t ch) {
        return static_cast<wchar_t>(std::towlower(ch));
    });
    return value;
}

const std::vector<std::wstring>& CoreSupportedExtensions() {
    static const std::vector<std::wstring> formats{
        L".jpg", L".jpeg", L".png", L".bmp", L".gif", L".tif", L".tiff",
        L".webp", L".heic", L".heif", L".ico"
    };
    return formats;
}

const std::vector<std::wstring>& ExtensionSupportedCandidates() {
    static const std::vector<std::wstring> formats{
        L".avif", L".svg", L".psd", L".psb", L".cr2", L".cr3", L".nef",
        L".arw", L".dng", L".rw2", L".orf", L".raf", L".exr", L".hdr"
    };
    return formats;
}

bool IsCoreSupportedExtension(std::wstring_view ext) {
    const std::wstring normalized = NormalizeExtension(ext);
    const auto& formats = CoreSupportedExtensions();
    return std::find(formats.begin(), formats.end(), normalized) != formats.end();
}

bool IsExtensionCandidate(std::wstring_view ext) {
    const std::wstring normalized = NormalizeExtension(ext);
    const auto& formats = ExtensionSupportedCandidates();
    return std::find(formats.begin(), formats.end(), normalized) != formats.end();
}

}  // namespace cpictures
```

- [ ] **Step 6: 写当前源文件并接入 CMake**

Create `src/platform/window_metrics.cpp`:

```cpp
#include "cpictures/geometry.h"
```

Create `src/platform/path_utils.cpp`:

```cpp
#include <filesystem>
```

Modify `CMakeLists.txt` before `include(CTest)`:

```cmake
add_library(cpictures_core STATIC
  src/image/supported_formats.cpp
  src/platform/path_utils.cpp
  src/platform/window_metrics.cpp
)
target_compile_features(cpictures_core PUBLIC cxx_std_20)
target_include_directories(cpictures_core PUBLIC include)
target_compile_definitions(cpictures_core PRIVATE UNICODE _UNICODE NOMINMAX WIN32_LEAN_AND_MEAN)
```

Modify the `cpictures_core_tests` target:

```cmake
  target_include_directories(cpictures_core_tests PRIVATE include)
  target_link_libraries(cpictures_core_tests PRIVATE cpictures_core)
```

- [ ] **Step 7: 运行测试**

Run:

```powershell
cmake --build --preset debug
ctest --preset debug
```

Expected:

```text
100% tests passed
```

- [ ] **Step 8: 提交**

Run:

```powershell
git add include src tests CMakeLists.txt
git commit -m "feat: add core viewer logic"
```

## Task 3: 图片文档和同目录图片列表

**Files:**
- Create: `include/cpictures/image_document.h`
- Create: `include/cpictures/image_list.h`
- Create: `src/image/image_document.cpp`
- Create: `src/image/image_list.cpp`
- Modify: `CMakeLists.txt`
- Modify: `tests/core_tests.cpp`

**Interfaces:**
- Produces: `ImageDocumentMetadata` 保存路径、文件名、尺寸、扩展名。
- Produces: `ImageList::LoadFromFile(std::filesystem::path) -> ImageList`。
- Produces: `ImageList::Current() const -> const std::filesystem::path&`、`Next()`、`Previous()`、`Index()`、`Count()`。

- [ ] **Step 1: 写失败测试**

Append to `tests/core_tests.cpp` before `main()`:

```cpp
#include <filesystem>
#include <fstream>

#include "cpictures/image_list.h"

namespace {
void WriteTinyFile(const std::filesystem::path& path) {
    std::ofstream out(path, std::ios::binary);
    out << "x";
}

void TestImageListNaturalOrder() {
    const auto dir = std::filesystem::temp_directory_path() / L"cpictures_list_test";
    std::filesystem::remove_all(dir);
    std::filesystem::create_directories(dir);
    WriteTinyFile(dir / L"10.jpg");
    WriteTinyFile(dir / L"2.jpg");
    WriteTinyFile(dir / L"1.jpg");
    WriteTinyFile(dir / L"notes.txt");

    cpictures::ImageList list = cpictures::ImageList::LoadFromFile(dir / L"2.jpg");
    Expect(list.Count() == 3, "image list counts supported files");
    Expect(list.Index() == 1, "image list locates current file");
    Expect(list.Current().filename() == L"2.jpg", "current image is requested file");
    Expect(list.Next().filename() == L"10.jpg", "next image follows natural order");
    Expect(list.Previous().filename() == L"2.jpg", "previous returns to requested file");
    std::filesystem::remove_all(dir);
}
}
```

Update `main()` to call:

```cpp
    TestImageListNaturalOrder();
```

- [ ] **Step 2: 运行测试并确认失败**

Run:

```powershell
cmake --build --preset debug
ctest --preset debug
```

Expected:

```text
error C2039: 'ImageList': is not a member of 'cpictures'
```

- [ ] **Step 3: 写图片文档元数据**

Create `include/cpictures/image_document.h`:

```cpp
#pragma once

#include <filesystem>
#include <string>

#include "cpictures/geometry.h"

namespace cpictures {

struct ImageDocumentMetadata {
    std::filesystem::path path;
    std::wstring fileName;
    std::wstring extension;
    SizeI pixelSize;
    int frameCount = 1;
    bool animated = false;
};

}  // namespace cpictures
```

Create `src/image/image_document.cpp`:

```cpp
#include "cpictures/image_document.h"
```

- [ ] **Step 4: 写图片列表**

Create `include/cpictures/image_list.h`:

```cpp
#pragma once

#include <filesystem>
#include <vector>

namespace cpictures {

class ImageList {
public:
    static ImageList LoadFromFile(const std::filesystem::path& path);

    const std::filesystem::path& Current() const;
    const std::filesystem::path& Next();
    const std::filesystem::path& Previous();
    size_t Index() const;
    size_t Count() const;
    bool Empty() const;

private:
    std::vector<std::filesystem::path> files_;
    size_t index_ = 0;
};

}  // namespace cpictures
```

Create `src/image/image_list.cpp`:

```cpp
#include "cpictures/image_list.h"

#include <algorithm>
#include <stdexcept>

#include "cpictures/natural_sort.h"
#include "cpictures/supported_formats.h"

namespace cpictures {
namespace {
bool IsKnownImagePath(const std::filesystem::path& path) {
    const std::wstring ext = path.extension().wstring();
    return IsCoreSupportedExtension(ext) || IsExtensionCandidate(ext);
}
}

ImageList ImageList::LoadFromFile(const std::filesystem::path& path) {
    ImageList list;
    const auto absolute = std::filesystem::absolute(path);
    const auto directory = absolute.parent_path();
    if (!std::filesystem::exists(absolute)) {
        list.files_.push_back(absolute);
        return list;
    }
    for (const auto& entry : std::filesystem::directory_iterator(directory)) {
        if (!entry.is_regular_file()) {
            continue;
        }
        if (IsKnownImagePath(entry.path())) {
            list.files_.push_back(std::filesystem::absolute(entry.path()));
        }
    }
    std::sort(list.files_.begin(), list.files_.end(), [](const auto& left, const auto& right) {
        return NaturalLess(left.filename().wstring(), right.filename().wstring());
    });
    const auto it = std::find(list.files_.begin(), list.files_.end(), absolute);
    if (it != list.files_.end()) {
        list.index_ = static_cast<size_t>(std::distance(list.files_.begin(), it));
    } else {
        list.files_.push_back(absolute);
        list.index_ = list.files_.size() - 1;
    }
    return list;
}

const std::filesystem::path& ImageList::Current() const {
    if (files_.empty()) {
        throw std::runtime_error("ImageList is empty");
    }
    return files_[index_];
}

const std::filesystem::path& ImageList::Next() {
    if (files_.empty()) {
        throw std::runtime_error("ImageList is empty");
    }
    index_ = (index_ + 1) % files_.size();
    return Current();
}

const std::filesystem::path& ImageList::Previous() {
    if (files_.empty()) {
        throw std::runtime_error("ImageList is empty");
    }
    index_ = index_ == 0 ? files_.size() - 1 : index_ - 1;
    return Current();
}

size_t ImageList::Index() const {
    return index_;
}

size_t ImageList::Count() const {
    return files_.size();
}

bool ImageList::Empty() const {
    return files_.empty();
}

}  // namespace cpictures
```

- [ ] **Step 5: 接入 CMake**

Modify `CMakeLists.txt` `cpictures_core` source list:

```cmake
add_library(cpictures_core STATIC
  src/image/image_document.cpp
  src/image/image_list.cpp
  src/image/supported_formats.cpp
  src/platform/path_utils.cpp
  src/platform/window_metrics.cpp
)
```

- [ ] **Step 6: 运行测试**

Run:

```powershell
cmake --build --preset debug
ctest --preset debug
```

Expected:

```text
100% tests passed
```

- [ ] **Step 7: 提交**

Run:

```powershell
git add CMakeLists.txt include/cpictures/image_document.h include/cpictures/image_list.h src/image tests/core_tests.cpp
git commit -m "feat: add directory image list"
```

## Task 4: WIC 解码器和图片加载

**Files:**
- Create: `src/image/wic_decoder.h`
- Create: `src/image/wic_decoder.cpp`
- Create: `tests/wic_tests.cpp`
- Modify: `CMakeLists.txt`

**Interfaces:**
- Produces: `DecodedImage { SizeI size; std::vector<std::byte> bgra; unsigned stride; }`。
- Produces: `WicDecoder::Decode(std::filesystem::path) -> DecodedImage`。

- [ ] **Step 1: 写 WIC 测试**

Create `tests/wic_tests.cpp`:

```cpp
#include <array>
#include <cstddef>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <iostream>

#include "image/wic_decoder.h"

namespace {
void Expect(bool condition, const char* message) {
    if (!condition) {
        std::cerr << "FAIL: " << message << "\n";
        std::exit(1);
    }
}

void WriteTinyBmp(const std::filesystem::path& path) {
    const std::array<unsigned char, 70> bmp = {
        0x42,0x4D,0x46,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x36,0x00,0x00,0x00,
        0x28,0x00,0x00,0x00,0x02,0x00,0x00,0x00,0x02,0x00,0x00,0x00,0x01,0x00,
        0x20,0x00,0x00,0x00,0x00,0x00,0x10,0x00,0x00,0x00,0x13,0x0B,0x00,0x00,
        0x13,0x0B,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,
        0x00,0x00,0xFF,0xFF,0x00,0xFF,0x00,0xFF,0xFF,0x00,0x00,0xFF,0xFF,0xFF,0xFF,0xFF
    };
    std::ofstream out(path, std::ios::binary);
    out.write(reinterpret_cast<const char*>(bmp.data()), static_cast<std::streamsize>(bmp.size()));
}
}

int main() {
    const auto path = std::filesystem::temp_directory_path() / L"cpictures_tiny.bmp";
    WriteTinyBmp(path);
    cpictures::WicDecoder decoder;
    const cpictures::DecodedImage image = decoder.Decode(path);
    Expect(image.size.width == 2, "decoded width");
    Expect(image.size.height == 2, "decoded height");
    Expect(image.stride == 8, "decoded stride");
    Expect(image.bgra.size() == 16, "decoded bgra bytes");
    std::filesystem::remove(path);
    std::cout << "wic tests passed\n";
    return 0;
}
```

- [ ] **Step 2: 加测试目标并确认失败**

Modify `CMakeLists.txt` test block:

```cmake
  add_executable(cpictures_wic_tests tests/wic_tests.cpp src/image/wic_decoder.cpp)
  target_compile_features(cpictures_wic_tests PRIVATE cxx_std_20)
  target_include_directories(cpictures_wic_tests PRIVATE include src)
  target_compile_definitions(cpictures_wic_tests PRIVATE UNICODE _UNICODE NOMINMAX WIN32_LEAN_AND_MEAN)
  target_link_libraries(cpictures_wic_tests PRIVATE cpictures_core windowscodecs ole32)
  add_test(NAME cpictures_wic_tests COMMAND cpictures_wic_tests)
```

Run:

```powershell
cmake --build --preset debug
ctest --preset debug
```

Expected:

```text
fatal error C1083: Cannot open include file: 'image/wic_decoder.h'
```

- [ ] **Step 3: 写 WIC 解码器**

Create `src/image/wic_decoder.h`:

```cpp
#pragma once

#include <cstddef>
#include <filesystem>
#include <vector>

#include "cpictures/geometry.h"

namespace cpictures {

struct DecodedImage {
    SizeI size;
    unsigned stride = 0;
    std::vector<std::byte> bgra;
};

class WicDecoder {
public:
    WicDecoder();
    ~WicDecoder();
    WicDecoder(const WicDecoder&) = delete;
    WicDecoder& operator=(const WicDecoder&) = delete;

    DecodedImage Decode(const std::filesystem::path& path) const;

private:
    bool ownsComApartment_ = false;
};

}  // namespace cpictures
```

Create `src/image/wic_decoder.cpp`:

```cpp
#include "image/wic_decoder.h"

#include <stdexcept>
#include <string>
#include <wincodec.h>
#include <wrl/client.h>

namespace cpictures {
namespace {
void ThrowIfFailed(HRESULT hr, const char* message) {
    if (FAILED(hr)) {
        throw std::runtime_error(message);
    }
}
}

WicDecoder::WicDecoder() {
    const HRESULT hr = CoInitializeEx(nullptr, COINIT_APARTMENTTHREADED);
    ownsComApartment_ = SUCCEEDED(hr);
    if (FAILED(hr) && hr != RPC_E_CHANGED_MODE) {
        throw std::runtime_error("CoInitializeEx failed");
    }
}

WicDecoder::~WicDecoder() {
    if (ownsComApartment_) {
        CoUninitialize();
    }
}

DecodedImage WicDecoder::Decode(const std::filesystem::path& path) const {
    Microsoft::WRL::ComPtr<IWICImagingFactory> factory;
    ThrowIfFailed(CoCreateInstance(CLSID_WICImagingFactory, nullptr, CLSCTX_INPROC_SERVER,
                                   IID_PPV_ARGS(&factory)),
                  "CoCreateInstance CLSID_WICImagingFactory failed");

    Microsoft::WRL::ComPtr<IWICBitmapDecoder> decoder;
    ThrowIfFailed(factory->CreateDecoderFromFilename(path.c_str(), nullptr, GENERIC_READ,
                                                     WICDecodeMetadataCacheOnDemand, &decoder),
                  "CreateDecoderFromFilename failed");

    Microsoft::WRL::ComPtr<IWICBitmapFrameDecode> frame;
    ThrowIfFailed(decoder->GetFrame(0, &frame), "GetFrame failed");

    UINT width = 0;
    UINT height = 0;
    ThrowIfFailed(frame->GetSize(&width, &height), "GetSize failed");

    Microsoft::WRL::ComPtr<IWICFormatConverter> converter;
    ThrowIfFailed(factory->CreateFormatConverter(&converter), "CreateFormatConverter failed");
    ThrowIfFailed(converter->Initialize(frame.Get(), GUID_WICPixelFormat32bppPBGRA,
                                        WICBitmapDitherTypeNone, nullptr, 0.0,
                                        WICBitmapPaletteTypeCustom),
                  "FormatConverter Initialize failed");

    DecodedImage result;
    result.size = {static_cast<int>(width), static_cast<int>(height)};
    result.stride = width * 4;
    result.bgra.resize(static_cast<size_t>(result.stride) * height);
    ThrowIfFailed(converter->CopyPixels(nullptr, result.stride,
                                        static_cast<UINT>(result.bgra.size()),
                                        reinterpret_cast<BYTE*>(result.bgra.data())),
                  "CopyPixels failed");
    return result;
}

}  // namespace cpictures
```

- [ ] **Step 4: 运行测试**

Run:

```powershell
cmake --build --preset debug
ctest --preset debug
```

Expected:

```text
100% tests passed
```

- [ ] **Step 5: 提交**

Run:

```powershell
git add CMakeLists.txt src/image/wic_decoder.* tests/wic_tests.cpp
git commit -m "feat: add WIC decoder"
```

## Task 5: Win32 无 chrome 窗口和主程序入口

**Files:**
- Create: `src/main.cpp`
- Create: `src/app/app.h`
- Create: `src/app/app.cpp`
- Create: `src/viewer/viewer_window.h`
- Create: `src/viewer/viewer_window.cpp`
- Modify: `CMakeLists.txt`

**Interfaces:**
- Consumes: `ImageList`、`WicDecoder`。
- Produces: `int RunApplication(HINSTANCE, int, std::wstring_view)`。
- Produces: `ViewerWindow::CreateAndShow(const std::filesystem::path&) -> int`。

- [ ] **Step 1: 写主程序入口**

Create `src/main.cpp`:

```cpp
#include <windows.h>

#include "app/app.h"

int WINAPI wWinMain(HINSTANCE instance, HINSTANCE, PWSTR commandLine, int showCommand) {
    (void)commandLine;
    return cpictures::RunApplication(instance, showCommand, GetCommandLineW());
}
```

Create `src/app/app.h`:

```cpp
#pragma once

#include <string_view>
#include <windows.h>

namespace cpictures {

int RunApplication(HINSTANCE instance, int showCommand, std::wstring_view commandLine);

}  // namespace cpictures
```

Create `src/app/app.cpp`:

```cpp
#include "app/app.h"

#include <shellapi.h>
#include <string>

#include "viewer/viewer_window.h"

namespace cpictures {

int RunApplication(HINSTANCE instance, int showCommand, std::wstring_view commandLine) {
    int argc = 0;
    PWSTR* argv = CommandLineToArgvW(commandLine.data(), &argc);
    std::wstring path;
    if (argv && argc > 1) {
        path = argv[1];
    }
    if (argv) {
        LocalFree(argv);
    }
    if (path.empty()) {
        MessageBoxW(nullptr, L"请传入图片路径。", L"cpictures", MB_OK | MB_ICONINFORMATION);
        return 1;
    }
    ViewerWindow window(instance, showCommand);
    return window.CreateAndShow(path);
}

}  // namespace cpictures
```

- [ ] **Step 2: 写窗口类第一阶段实现**

Create `src/viewer/viewer_window.h`:

```cpp
#pragma once

#include <filesystem>
#include <windows.h>

#include "cpictures/image_list.h"
#include "cpictures/view_state.h"
#include "image/wic_decoder.h"

namespace cpictures {

class ViewerWindow {
public:
    ViewerWindow(HINSTANCE instance, int showCommand);
    int CreateAndShow(const std::filesystem::path& path);

private:
    static LRESULT CALLBACK WindowProc(HWND hwnd, UINT message, WPARAM wparam, LPARAM lparam);
    LRESULT HandleMessage(UINT message, WPARAM wparam, LPARAM lparam);
    void Close();
    void ToggleOverlay();
    void BeginMoveDrag();

    HINSTANCE instance_ = nullptr;
    int showCommand_ = SW_SHOWNORMAL;
    HWND hwnd_ = nullptr;
    ImageList imageList_;
    ViewState viewState_;
    WicDecoder decoder_;
};

}  // namespace cpictures
```

Create `src/viewer/viewer_window.cpp`:

```cpp
#include "viewer/viewer_window.h"

#include <stdexcept>
#include <string>

namespace cpictures {
namespace {
constexpr wchar_t kWindowClassName[] = L"cpictures.viewer";
}

ViewerWindow::ViewerWindow(HINSTANCE instance, int showCommand)
    : instance_(instance), showCommand_(showCommand) {}

int ViewerWindow::CreateAndShow(const std::filesystem::path& path) {
    imageList_ = ImageList::LoadFromFile(path);

    WNDCLASSEXW wc{};
    wc.cbSize = sizeof(wc);
    wc.hInstance = instance_;
    wc.lpfnWndProc = ViewerWindow::WindowProc;
    wc.lpszClassName = kWindowClassName;
    wc.hCursor = LoadCursor(nullptr, IDC_ARROW);
    RegisterClassExW(&wc);

    hwnd_ = CreateWindowExW(
        WS_EX_APPWINDOW,
        kWindowClassName,
        L"cpictures",
        WS_POPUP,
        CW_USEDEFAULT,
        CW_USEDEFAULT,
        640,
        480,
        nullptr,
        nullptr,
        instance_,
        this);

    if (!hwnd_) {
        MessageBoxW(nullptr, L"创建窗口失败。", L"cpictures", MB_OK | MB_ICONERROR);
        return 1;
    }

    ShowWindow(hwnd_, showCommand_);
    UpdateWindow(hwnd_);

    MSG msg{};
    while (GetMessageW(&msg, nullptr, 0, 0) > 0) {
        TranslateMessage(&msg);
        DispatchMessageW(&msg);
    }
    return static_cast<int>(msg.wParam);
}

LRESULT CALLBACK ViewerWindow::WindowProc(HWND hwnd, UINT message, WPARAM wparam, LPARAM lparam) {
    ViewerWindow* self = nullptr;
    if (message == WM_NCCREATE) {
        auto* create = reinterpret_cast<CREATESTRUCTW*>(lparam);
        self = static_cast<ViewerWindow*>(create->lpCreateParams);
        SetWindowLongPtrW(hwnd, GWLP_USERDATA, reinterpret_cast<LONG_PTR>(self));
        self->hwnd_ = hwnd;
    } else {
        self = reinterpret_cast<ViewerWindow*>(GetWindowLongPtrW(hwnd, GWLP_USERDATA));
    }
    if (self) {
        return self->HandleMessage(message, wparam, lparam);
    }
    return DefWindowProcW(hwnd, message, wparam, lparam);
}

LRESULT ViewerWindow::HandleMessage(UINT message, WPARAM wparam, LPARAM lparam) {
    switch (message) {
    case WM_KEYDOWN:
        if (wparam == VK_ESCAPE) {
            Close();
            return 0;
        }
        break;
    case WM_LBUTTONUP:
        ToggleOverlay();
        InvalidateRect(hwnd_, nullptr, FALSE);
        return 0;
    case WM_LBUTTONDOWN:
        SetCapture(hwnd_);
        return 0;
    case WM_MOUSEMOVE:
        if ((wparam & MK_LBUTTON) != 0) {
            BeginMoveDrag();
            return 0;
        }
        break;
    case WM_PAINT: {
        PAINTSTRUCT ps{};
        BeginPaint(hwnd_, &ps);
        EndPaint(hwnd_, &ps);
        return 0;
    }
    case WM_DESTROY:
        PostQuitMessage(0);
        return 0;
    }
    return DefWindowProcW(hwnd_, message, wparam, lparam);
}

void ViewerWindow::Close() {
    DestroyWindow(hwnd_);
}

void ViewerWindow::ToggleOverlay() {
    viewState_.overlayVisible = !viewState_.overlayVisible;
}

void ViewerWindow::BeginMoveDrag() {
    ReleaseCapture();
    SendMessageW(hwnd_, WM_NCLBUTTONDOWN, HTCAPTION, 0);
}

}  // namespace cpictures
```

- [ ] **Step 3: 接入 CMake**

Modify `CMakeLists.txt` before `include(CTest)`:

```cmake
add_executable(cpictures WIN32
  src/main.cpp
  src/app/app.cpp
  src/image/wic_decoder.cpp
  src/viewer/viewer_window.cpp
)
target_compile_features(cpictures PRIVATE cxx_std_20)
target_include_directories(cpictures PRIVATE include src)
target_compile_definitions(cpictures PRIVATE UNICODE _UNICODE NOMINMAX WIN32_LEAN_AND_MEAN)
target_link_libraries(cpictures PRIVATE cpictures_core windowscodecs ole32 shell32 user32 gdi32)
```

- [ ] **Step 4: 构建并手动烟测窗口**

Run:

```powershell
cmake --build --preset debug
.\build\vs2022-x64\Debug\cpictures.exe C:\Windows\Web\Wallpaper\Windows\img0.jpg
```

Expected:

```text
应用打开无标题栏窗口；按 Esc 关闭。
```

- [ ] **Step 5: 提交**

Run:

```powershell
git add CMakeLists.txt src/main.cpp src/app src/viewer
git commit -m "feat: add borderless viewer window"
```

## Task 6: Direct2D 渲染、100% 贴合窗口、角标绘制

**Files:**
- Create: `src/render/d2d_renderer.h`
- Create: `src/render/d2d_renderer.cpp`
- Modify: `src/viewer/viewer_window.h`
- Modify: `src/viewer/viewer_window.cpp`
- Modify: `CMakeLists.txt`

**Interfaces:**
- Consumes: `DecodedImage`、`ViewState`、`BuildOverlayText`。
- Produces: `D2DRenderer::Render(HWND, DecodedImage, ViewState, std::wstring overlayText)`。
- Produces: `ViewerWindow::LoadCurrentImage()` 调整窗口客户区为 `FitImageWindow(imageSize, workArea)`。

- [ ] **Step 1: 写渲染器头文件**

Create `src/render/d2d_renderer.h`:

```cpp
#pragma once

#include <string>
#include <windows.h>
#include <d2d1.h>
#include <d2d1helper.h>
#include <dwrite.h>
#include <wrl/client.h>

#include "cpictures/view_state.h"
#include "image/wic_decoder.h"

namespace cpictures {

class D2DRenderer {
public:
    D2DRenderer();
    void EnsureDevice(HWND hwnd);
    void Resize(HWND hwnd);
    void SetImage(const DecodedImage& image);
    void Render(HWND hwnd, const ViewState& state, const std::wstring& overlayText);

private:
    Microsoft::WRL::ComPtr<ID2D1Factory> factory_;
    Microsoft::WRL::ComPtr<IDWriteFactory> writeFactory_;
    Microsoft::WRL::ComPtr<ID2D1HwndRenderTarget> target_;
    Microsoft::WRL::ComPtr<ID2D1Bitmap> bitmap_;
    Microsoft::WRL::ComPtr<ID2D1SolidColorBrush> textBrush_;
    Microsoft::WRL::ComPtr<ID2D1SolidColorBrush> badgeBrush_;
    Microsoft::WRL::ComPtr<IDWriteTextFormat> textFormat_;
};

}  // namespace cpictures
```

- [ ] **Step 2: 写渲染器实现**

Create `src/render/d2d_renderer.cpp`:

```cpp
#include "render/d2d_renderer.h"

#include <stdexcept>

namespace cpictures {
namespace {
void ThrowIfFailed(HRESULT hr, const char* message) {
    if (FAILED(hr)) {
        throw std::runtime_error(message);
    }
}
}

D2DRenderer::D2DRenderer() {
    ThrowIfFailed(D2D1CreateFactory(D2D1_FACTORY_TYPE_SINGLE_THREADED, factory_.GetAddressOf()),
                  "D2D1CreateFactory failed");
    ThrowIfFailed(DWriteCreateFactory(DWRITE_FACTORY_TYPE_SHARED, __uuidof(IDWriteFactory),
                                      reinterpret_cast<IUnknown**>(writeFactory_.GetAddressOf())),
                  "DWriteCreateFactory failed");
    ThrowIfFailed(writeFactory_->CreateTextFormat(L"Segoe UI", nullptr, DWRITE_FONT_WEIGHT_NORMAL,
                                                  DWRITE_FONT_STYLE_NORMAL, DWRITE_FONT_STRETCH_NORMAL,
                                                  12.0f, L"zh-CN", textFormat_.GetAddressOf()),
                  "CreateTextFormat failed");
}

void D2DRenderer::EnsureDevice(HWND hwnd) {
    if (target_) {
        return;
    }
    RECT rc{};
    GetClientRect(hwnd, &rc);
    const D2D1_SIZE_U size = D2D1::SizeU(static_cast<UINT32>(rc.right - rc.left),
                                        static_cast<UINT32>(rc.bottom - rc.top));
    ThrowIfFailed(factory_->CreateHwndRenderTarget(D2D1::RenderTargetProperties(),
                                                  D2D1::HwndRenderTargetProperties(hwnd, size),
                                                  target_.GetAddressOf()),
                  "CreateHwndRenderTarget failed");
    ThrowIfFailed(target_->CreateSolidColorBrush(D2D1::ColorF(1.0f, 1.0f, 1.0f, 0.90f),
                                                 textBrush_.GetAddressOf()),
                  "Create text brush failed");
    ThrowIfFailed(target_->CreateSolidColorBrush(D2D1::ColorF(0.0f, 0.0f, 0.0f, 0.34f),
                                                 badgeBrush_.GetAddressOf()),
                  "Create badge brush failed");
}

void D2DRenderer::Resize(HWND hwnd) {
    if (!target_) {
        return;
    }
    RECT rc{};
    GetClientRect(hwnd, &rc);
    target_->Resize(D2D1::SizeU(static_cast<UINT32>(rc.right - rc.left),
                                static_cast<UINT32>(rc.bottom - rc.top)));
}

void D2DRenderer::SetImage(const DecodedImage& image) {
    if (!target_) {
        return;
    }
    const D2D1_BITMAP_PROPERTIES props = D2D1::BitmapProperties(
        D2D1::PixelFormat(DXGI_FORMAT_B8G8R8A8_UNORM, D2D1_ALPHA_MODE_PREMULTIPLIED));
    ThrowIfFailed(target_->CreateBitmap(
                      D2D1::SizeU(static_cast<UINT32>(image.size.width),
                                  static_cast<UINT32>(image.size.height)),
                      image.bgra.data(),
                      image.stride,
                      props,
                      bitmap_.ReleaseAndGetAddressOf()),
                  "CreateBitmap failed");
}

void D2DRenderer::Render(HWND hwnd, const ViewState& state, const std::wstring& overlayText) {
    EnsureDevice(hwnd);
    target_->BeginDraw();
    target_->Clear(D2D1::ColorF(0, 0.0f));
    if (bitmap_) {
        const D2D1_SIZE_F imageSize = bitmap_->GetSize();
        RECT rc{};
        GetClientRect(hwnd, &rc);
        const float clientW = static_cast<float>(rc.right - rc.left);
        const float clientH = static_cast<float>(rc.bottom - rc.top);
        const float drawW = imageSize.width * static_cast<float>(state.zoom);
        const float drawH = imageSize.height * static_cast<float>(state.zoom);
        const D2D1_RECT_F dest = D2D1::RectF((clientW - drawW) * 0.5f,
                                            (clientH - drawH) * 0.5f,
                                            (clientW + drawW) * 0.5f,
                                            (clientH + drawH) * 0.5f);
        target_->DrawBitmap(bitmap_.Get(), dest, 1.0f, D2D1_BITMAP_INTERPOLATION_MODE_LINEAR);
    }
    if (state.overlayVisible && !overlayText.empty()) {
        const D2D1_RECT_F badge = D2D1::RectF(8.0f, 8.0f, 260.0f, 30.0f);
        RECT rc{};
        GetClientRect(hwnd, &rc);
        const float y = static_cast<float>(rc.bottom - rc.top) - 30.0f;
        const D2D1_RECT_F bottomBadge = D2D1::RectF(8.0f, y, 260.0f, y + 22.0f);
        target_->FillRoundedRectangle(D2D1::RoundedRect(bottomBadge, 4.0f, 4.0f), badgeBrush_.Get());
        target_->DrawTextW(overlayText.c_str(), static_cast<UINT32>(overlayText.size()), textFormat_.Get(),
                           D2D1::RectF(bottomBadge.left + 7.0f, bottomBadge.top + 3.0f,
                                       bottomBadge.right - 7.0f, bottomBadge.bottom),
                           textBrush_.Get());
    }
    const HRESULT hr = target_->EndDraw();
    if (hr == D2DERR_RECREATE_TARGET) {
        target_.Reset();
        bitmap_.Reset();
    }
}

}  // namespace cpictures
```

- [ ] **Step 3: 接入窗口加载和尺寸贴合**

Modify `src/viewer/viewer_window.h`:

```cpp
#include "render/d2d_renderer.h"

// 在 private 区增加：
void LoadCurrentImage();
SizeI WorkAreaForWindow() const;
std::wstring OverlayText() const;

DecodedImage decoded_;
D2DRenderer renderer_;
```

Modify `src/viewer/viewer_window.cpp`:

```cpp
#include "cpictures/geometry.h"
#include "cpictures/overlay.h"

// 在 CreateAndShow 中 CreateWindowExW 成功后、ShowWindow 前增加：
LoadCurrentImage();

// 在 WM_PAINT 分支替换 BeginPaint/EndPaint 中间逻辑：
renderer_.Render(hwnd_, viewState_, OverlayText());

// 增加 WM_SIZE 分支：
case WM_SIZE:
    renderer_.Resize(hwnd_);
    return 0;

// 文件末尾增加：
void ViewerWindow::LoadCurrentImage() {
    decoded_ = decoder_.Decode(imageList_.Current());
    const SizeI target = FitImageWindow(decoded_.size, WorkAreaForWindow());
    SetWindowPos(hwnd_, nullptr, 0, 0, target.width, target.height,
                 SWP_NOMOVE | SWP_NOZORDER | SWP_NOACTIVATE);
    RECT rc{};
    GetWindowRect(hwnd_, &rc);
    HMONITOR monitor = MonitorFromWindow(hwnd_, MONITOR_DEFAULTTONEAREST);
    MONITORINFO mi{sizeof(mi)};
    GetMonitorInfoW(monitor, &mi);
    const int x = mi.rcWork.left + ((mi.rcWork.right - mi.rcWork.left) - target.width) / 2;
    const int y = mi.rcWork.top + ((mi.rcWork.bottom - mi.rcWork.top) - target.height) / 2;
    SetWindowPos(hwnd_, nullptr, x, y, target.width, target.height,
                 SWP_NOZORDER | SWP_NOACTIVATE);
    renderer_.EnsureDevice(hwnd_);
    renderer_.SetImage(decoded_);
}

SizeI ViewerWindow::WorkAreaForWindow() const {
    HMONITOR monitor = MonitorFromWindow(hwnd_, MONITOR_DEFAULTTONEAREST);
    MONITORINFO mi{sizeof(mi)};
    GetMonitorInfoW(monitor, &mi);
    return {mi.rcWork.right - mi.rcWork.left, mi.rcWork.bottom - mi.rcWork.top};
}

std::wstring ViewerWindow::OverlayText() const {
    return BuildOverlayText(imageList_.Current().filename().wstring(),
                            static_cast<int>(imageList_.Index()),
                            static_cast<int>(imageList_.Count()));
}
```

- [ ] **Step 4: 接入 CMake**

Modify `CMakeLists.txt` target `cpictures` sources:

```cmake
add_executable(cpictures WIN32
  src/main.cpp
  src/app/app.cpp
  src/image/wic_decoder.cpp
  src/render/d2d_renderer.cpp
  src/viewer/viewer_window.cpp
)
```

Modify `cpictures` link libraries:

```cmake
target_link_libraries(cpictures PRIVATE cpictures_core windowscodecs d2d1 dwrite dxgi ole32 shell32 user32 gdi32)
```

- [ ] **Step 5: 构建并手动验证**

Run:

```powershell
cmake --build --preset debug
.\build\vs2022-x64\Debug\cpictures.exe C:\Windows\Web\Wallpaper\Windows\img0.jpg
```

Expected:

```text
窗口无标题栏、无边框；图片绘制出来；单击显示/隐藏左下角角标；Esc 关闭。
```

- [ ] **Step 6: 提交**

Run:

```powershell
git add src/render src/viewer CMakeLists.txt
git commit -m "feat: render image in borderless window"
```

## Task 7: 切图、缩放、全屏、临时旋转

**Files:**
- Modify: `include/cpictures/commands.h`
- Modify: `src/viewer/viewer_window.h`
- Modify: `src/viewer/viewer_window.cpp`
- Modify: `src/render/d2d_renderer.cpp`

**Interfaces:**
- Produces: `enum class Command`。
- Produces: `ViewerWindow::ExecuteCommand(Command)`。
- Produces: rotation display matrix; no file write.

- [ ] **Step 1: 写命令枚举**

Create `include/cpictures/commands.h`:

```cpp
#pragma once

namespace cpictures {

enum class Command {
    PreviousImage,
    NextImage,
    ZoomIn,
    ZoomOut,
    ActualSize,
    FitToScreen,
    ToggleFullscreen,
    RotateLeft,
    RotateRight,
    CopyFile,
    CopyPath,
    InstallOrUpdateFormats
};

}  // namespace cpictures
```

- [ ] **Step 2: 接入键盘和滚轮**

Modify `src/viewer/viewer_window.h` private methods:

```cpp
void ExecuteCommand(Command command);
void ShowNextImage();
void ShowPreviousImage();
void ApplyZoom(double factor);
void SetActualSize();
void SetFitToScreen();
void ToggleFullscreen();
void RotateBy(int degrees);

RECT restoreRect_{};
bool hasRestoreRect_ = false;
```

Add include:

```cpp
#include "cpictures/commands.h"
```

Modify `src/viewer/viewer_window.cpp` `HandleMessage`:

```cpp
case WM_KEYDOWN:
    if (wparam == VK_ESCAPE) { Close(); return 0; }
    if (wparam == VK_LEFT || wparam == VK_PRIOR) { ExecuteCommand(Command::PreviousImage); return 0; }
    if (wparam == VK_RIGHT || wparam == VK_NEXT) { ExecuteCommand(Command::NextImage); return 0; }
    if (wparam == L'1') { ExecuteCommand(Command::ActualSize); return 0; }
    if (wparam == L'0') { ExecuteCommand(Command::FitToScreen); return 0; }
    if (wparam == VK_F11) { ExecuteCommand(Command::ToggleFullscreen); return 0; }
    if ((GetKeyState(VK_CONTROL) & 0x8000) != 0 && wparam == L'L') { ExecuteCommand(Command::RotateLeft); return 0; }
    if ((GetKeyState(VK_CONTROL) & 0x8000) != 0 && wparam == L'R') { ExecuteCommand(Command::RotateRight); return 0; }
    if (wparam == VK_OEM_PLUS || wparam == VK_ADD) { ExecuteCommand(Command::ZoomIn); return 0; }
    if (wparam == VK_OEM_MINUS || wparam == VK_SUBTRACT) { ExecuteCommand(Command::ZoomOut); return 0; }
    break;
case WM_MOUSEWHEEL: {
    const short delta = GET_WHEEL_DELTA_WPARAM(wparam);
    if ((GET_KEYSTATE_WPARAM(wparam) & MK_CONTROL) != 0) {
        ExecuteCommand(delta > 0 ? Command::ZoomIn : Command::ZoomOut);
    } else {
        ExecuteCommand(delta > 0 ? Command::PreviousImage : Command::NextImage);
    }
    return 0;
}
```

- [ ] **Step 3: 实现命令**

Append to `src/viewer/viewer_window.cpp`:

```cpp
void ViewerWindow::ExecuteCommand(Command command) {
    switch (command) {
    case Command::PreviousImage: ShowPreviousImage(); break;
    case Command::NextImage: ShowNextImage(); break;
    case Command::ZoomIn: ApplyZoom(1.1); break;
    case Command::ZoomOut: ApplyZoom(1.0 / 1.1); break;
    case Command::ActualSize: SetActualSize(); break;
    case Command::FitToScreen: SetFitToScreen(); break;
    case Command::ToggleFullscreen: ToggleFullscreen(); break;
    case Command::RotateLeft: RotateBy(-90); break;
    case Command::RotateRight: RotateBy(90); break;
    case Command::CopyFile:
    case Command::CopyPath:
    case Command::InstallOrUpdateFormats:
        break;
    }
    InvalidateRect(hwnd_, nullptr, FALSE);
}

void ViewerWindow::ShowNextImage() {
    imageList_.Next();
    viewState_.zoom = 1.0;
    viewState_.rotationDegrees = 0;
    LoadCurrentImage();
}

void ViewerWindow::ShowPreviousImage() {
    imageList_.Previous();
    viewState_.zoom = 1.0;
    viewState_.rotationDegrees = 0;
    LoadCurrentImage();
}

void ViewerWindow::ApplyZoom(double factor) {
    viewState_.fitMode = FitMode::ActualSize;
    viewState_.zoom = std::clamp(viewState_.zoom * factor, 0.05, 32.0);
}

void ViewerWindow::SetActualSize() {
    viewState_.fitMode = FitMode::ActualSize;
    viewState_.zoom = 1.0;
}

void ViewerWindow::SetFitToScreen() {
    viewState_.fitMode = FitMode::FitToScreen;
    const SizeI work = WorkAreaForWindow();
    const SizeI fit = FitImageWindow(decoded_.size, work);
    viewState_.zoom = static_cast<double>(fit.width) / decoded_.size.width;
}

void ViewerWindow::ToggleFullscreen() {
    if (!viewState_.fullscreen) {
        GetWindowRect(hwnd_, &restoreRect_);
        hasRestoreRect_ = true;
        HMONITOR monitor = MonitorFromWindow(hwnd_, MONITOR_DEFAULTTONEAREST);
        MONITORINFO mi{sizeof(mi)};
        GetMonitorInfoW(monitor, &mi);
        SetWindowPos(hwnd_, HWND_TOP, mi.rcMonitor.left, mi.rcMonitor.top,
                     mi.rcMonitor.right - mi.rcMonitor.left,
                     mi.rcMonitor.bottom - mi.rcMonitor.top,
                     SWP_FRAMECHANGED);
        viewState_.fullscreen = true;
    } else if (hasRestoreRect_) {
        SetWindowPos(hwnd_, nullptr, restoreRect_.left, restoreRect_.top,
                     restoreRect_.right - restoreRect_.left,
                     restoreRect_.bottom - restoreRect_.top,
                     SWP_NOZORDER | SWP_FRAMECHANGED);
        viewState_.fullscreen = false;
    }
}

void ViewerWindow::RotateBy(int degrees) {
    viewState_.rotationDegrees = (viewState_.rotationDegrees + degrees + 360) % 360;
}
```

Add includes:

```cpp
#include <algorithm>
```

- [ ] **Step 4: 渲染器支持旋转**

In `src/render/d2d_renderer.cpp`, inside `Render` before `DrawBitmap`, replace direct `DrawBitmap` with transform:

```cpp
        const D2D1_POINT_2F center = D2D1::Point2F(clientW * 0.5f, clientH * 0.5f);
        const D2D1_MATRIX_3X2_F oldTransform = [] (ID2D1HwndRenderTarget* target) {
            D2D1_MATRIX_3X2_F transform{};
            target->GetTransform(&transform);
            return transform;
        }(target_.Get());
        target_->SetTransform(D2D1::Matrix3x2F::Rotation(static_cast<float>(state.rotationDegrees), center));
        target_->DrawBitmap(bitmap_.Get(), dest, 1.0f, D2D1_BITMAP_INTERPOLATION_MODE_LINEAR);
        target_->SetTransform(oldTransform);
```

- [ ] **Step 5: 构建并手动验证**

Run:

```powershell
cmake --build --preset debug
.\build\vs2022-x64\Debug\cpictures.exe C:\Windows\Web\Wallpaper\Windows\img0.jpg
```

Expected:

```text
滚轮切图；Ctrl+滚轮缩放；左右键切图；1 回到 100%；0 适应屏幕；F11 全屏；Ctrl+L/Ctrl+R 临时旋转；原文件未修改。
```

- [ ] **Step 6: 提交**

Run:

```powershell
git add include/cpictures/commands.h src/viewer src/render
git commit -m "feat: add viewer navigation controls"
```

## Task 8: 右键菜单和剪贴板复制

**Files:**
- Create: `src/platform/context_menu.h`
- Create: `src/platform/context_menu.cpp`
- Create: `src/platform/clipboard_service.h`
- Create: `src/platform/clipboard_service.cpp`
- Modify: `src/viewer/viewer_window.cpp`

**Interfaces:**
- Produces: `ShowContextMenu(HWND, POINT) -> std::optional<Command>`。
- Produces: `CopyFileToClipboard(HWND, std::filesystem::path)`。
- Produces: `CopyTextToClipboard(HWND, std::wstring)`。

- [ ] **Step 1: 写右键菜单**

Create `src/platform/context_menu.h`:

```cpp
#pragma once

#include <optional>
#include <windows.h>

#include "cpictures/commands.h"

namespace cpictures {

std::optional<Command> ShowContextMenu(HWND owner, POINT screenPoint);

}  // namespace cpictures
```

Create `src/platform/context_menu.cpp`:

```cpp
#include "platform/context_menu.h"

namespace cpictures {
namespace {
constexpr UINT kRotateLeft = 1001;
constexpr UINT kRotateRight = 1002;
constexpr UINT kCopyFile = 1003;
constexpr UINT kCopyPath = 1004;
constexpr UINT kActualSize = 1005;
constexpr UINT kFitToScreen = 1006;
constexpr UINT kFullscreen = 1007;
constexpr UINT kInstallFormats = 1008;
}

std::optional<Command> ShowContextMenu(HWND owner, POINT screenPoint) {
    HMENU menu = CreatePopupMenu();
    AppendMenuW(menu, MF_STRING, kRotateLeft, L"向左旋转\tCtrl+L");
    AppendMenuW(menu, MF_STRING, kRotateRight, L"向右旋转\tCtrl+R");
    AppendMenuW(menu, MF_SEPARATOR, 0, nullptr);
    AppendMenuW(menu, MF_STRING, kCopyFile, L"复制\tCtrl+C");
    AppendMenuW(menu, MF_STRING, kCopyPath, L"复制路径\tCtrl+Shift+C");
    AppendMenuW(menu, MF_SEPARATOR, 0, nullptr);
    AppendMenuW(menu, MF_STRING, kActualSize, L"实际大小\t1");
    AppendMenuW(menu, MF_STRING, kFitToScreen, L"适应屏幕\t0");
    AppendMenuW(menu, MF_STRING, kFullscreen, L"全屏\tF11");
    AppendMenuW(menu, MF_SEPARATOR, 0, nullptr);
    AppendMenuW(menu, MF_STRING, kInstallFormats, L"安装/更新格式支持...");
    const UINT selected = TrackPopupMenu(menu, TPM_RETURNCMD | TPM_RIGHTBUTTON,
                                         screenPoint.x, screenPoint.y, 0, owner, nullptr);
    DestroyMenu(menu);
    switch (selected) {
    case kRotateLeft: return Command::RotateLeft;
    case kRotateRight: return Command::RotateRight;
    case kCopyFile: return Command::CopyFile;
    case kCopyPath: return Command::CopyPath;
    case kActualSize: return Command::ActualSize;
    case kFitToScreen: return Command::FitToScreen;
    case kFullscreen: return Command::ToggleFullscreen;
    case kInstallFormats: return Command::InstallOrUpdateFormats;
    default: return std::nullopt;
    }
}

}  // namespace cpictures
```

- [ ] **Step 2: 写剪贴板服务**

Create `src/platform/clipboard_service.h`:

```cpp
#pragma once

#include <filesystem>
#include <string>
#include <windows.h>

namespace cpictures {

bool CopyFileToClipboard(HWND owner, const std::filesystem::path& path);
bool CopyTextToClipboard(HWND owner, const std::wstring& text);

}  // namespace cpictures
```

Create `src/platform/clipboard_service.cpp`:

```cpp
#include "platform/clipboard_service.h"

#include <cstring>
#include <shellapi.h>

namespace cpictures {

bool CopyFileToClipboard(HWND owner, const std::filesystem::path& path) {
    const std::wstring fullPath = std::filesystem::absolute(path).wstring();
    const size_t bytes = sizeof(DROPFILES) + (fullPath.size() + 2) * sizeof(wchar_t);
    HGLOBAL memory = GlobalAlloc(GMEM_MOVEABLE | GMEM_ZEROINIT, bytes);
    if (!memory) {
        return false;
    }
    auto* drop = static_cast<DROPFILES*>(GlobalLock(memory));
    drop->pFiles = sizeof(DROPFILES);
    drop->fWide = TRUE;
    auto* fileList = reinterpret_cast<wchar_t*>(reinterpret_cast<BYTE*>(drop) + sizeof(DROPFILES));
    std::memcpy(fileList, fullPath.c_str(), fullPath.size() * sizeof(wchar_t));
    GlobalUnlock(memory);

    if (!OpenClipboard(owner)) {
        GlobalFree(memory);
        return false;
    }
    EmptyClipboard();
    SetClipboardData(CF_HDROP, memory);
    CloseClipboard();
    return true;
}

bool CopyTextToClipboard(HWND owner, const std::wstring& text) {
    const size_t bytes = (text.size() + 1) * sizeof(wchar_t);
    HGLOBAL memory = GlobalAlloc(GMEM_MOVEABLE | GMEM_ZEROINIT, bytes);
    if (!memory) {
        return false;
    }
    auto* buffer = static_cast<wchar_t*>(GlobalLock(memory));
    std::memcpy(buffer, text.c_str(), bytes);
    GlobalUnlock(memory);

    if (!OpenClipboard(owner)) {
        GlobalFree(memory);
        return false;
    }
    EmptyClipboard();
    SetClipboardData(CF_UNICODETEXT, memory);
    CloseClipboard();
    return true;
}

}  // namespace cpictures
```

- [ ] **Step 3: 接入窗口**

Modify `src/viewer/viewer_window.cpp` includes:

```cpp
#include "platform/clipboard_service.h"
#include "platform/context_menu.h"
```

Add cases:

```cpp
case WM_CONTEXTMENU: {
    POINT pt{GET_X_LPARAM(lparam), GET_Y_LPARAM(lparam)};
    if (pt.x == -1 && pt.y == -1) {
        RECT rc{};
        GetWindowRect(hwnd_, &rc);
        pt = {rc.left + 24, rc.top + 24};
    }
    if (auto command = ShowContextMenu(hwnd_, pt)) {
        ExecuteCommand(*command);
    }
    return 0;
}
```

Add to `WM_KEYDOWN` before `break`:

```cpp
    if ((GetKeyState(VK_CONTROL) & 0x8000) != 0 && wparam == L'C') {
        if ((GetKeyState(VK_SHIFT) & 0x8000) != 0) {
            ExecuteCommand(Command::CopyPath);
        } else {
            ExecuteCommand(Command::CopyFile);
        }
        return 0;
    }
```

Extend `ExecuteCommand`:

```cpp
    case Command::CopyFile:
        CopyFileToClipboard(hwnd_, imageList_.Current());
        break;
    case Command::CopyPath:
        CopyTextToClipboard(hwnd_, std::filesystem::absolute(imageList_.Current()).wstring());
        break;
    case Command::InstallOrUpdateFormats:
        MessageBoxW(hwnd_, L"扩展格式支持将在格式组件中安装或更新。", L"cpictures", MB_OK | MB_ICONINFORMATION);
        break;
```

Add include:

```cpp
#include <windowsx.h>
```

- [ ] **Step 4: 接入 CMake**

Modify `CMakeLists.txt` target `cpictures` sources:

```cmake
add_executable(cpictures WIN32
  src/main.cpp
  src/app/app.cpp
  src/image/wic_decoder.cpp
  src/platform/clipboard_service.cpp
  src/platform/context_menu.cpp
  src/render/d2d_renderer.cpp
  src/viewer/viewer_window.cpp
)
```

Keep link libraries including `shell32 user32`.

- [ ] **Step 5: 构建并手动验证**

Run:

```powershell
cmake --build --preset debug
.\build\vs2022-x64\Debug\cpictures.exe C:\Windows\Web\Wallpaper\Windows\img0.jpg
```

Expected:

```text
右键菜单只含指定项目；Ctrl+C 后可在资源管理器文件夹粘贴文件；Ctrl+Shift+C 后可粘贴完整路径文本。
```

- [ ] **Step 6: 提交**

Run:

```powershell
git add src/platform src/viewer CMakeLists.txt
git commit -m "feat: add context menu and clipboard actions"
```

## Task 9: 预读缓存和快速切换

**Files:**
- Create: `src/image/prefetcher.h`
- Create: `src/image/prefetcher.cpp`
- Modify: `src/viewer/viewer_window.h`
- Modify: `src/viewer/viewer_window.cpp`
- Modify: `CMakeLists.txt`

**Interfaces:**
- Produces: `Prefetcher::Warm(path previous, path next)` 后台解码前后图。
- Produces: `Prefetcher::Take(path) -> std::optional<DecodedImage>`。

- [ ] **Step 1: 写预读器头文件**

Create `src/image/prefetcher.h`:

```cpp
#pragma once

#include <filesystem>
#include <mutex>
#include <optional>
#include <thread>
#include <unordered_map>
#include <vector>

#include "image/wic_decoder.h"

namespace cpictures {

class Prefetcher {
public:
    ~Prefetcher();
    void Warm(const std::filesystem::path& first, const std::filesystem::path& second);
    std::optional<DecodedImage> Take(const std::filesystem::path& path);
    void Stop();

private:
    void DecodeOne(std::filesystem::path path);

    std::mutex mutex_;
    std::unordered_map<std::wstring, DecodedImage> cache_;
    std::vector<std::thread> workers_;
    bool stopped_ = false;
};

}  // namespace cpictures
```

- [ ] **Step 2: 写预读器实现**

Create `src/image/prefetcher.cpp`:

```cpp
#include "image/prefetcher.h"

namespace cpictures {

Prefetcher::~Prefetcher() {
    Stop();
}

void Prefetcher::Warm(const std::filesystem::path& first, const std::filesystem::path& second) {
    Stop();
    {
        std::lock_guard lock(mutex_);
        stopped_ = false;
        cache_.clear();
    }
    workers_.emplace_back([this, first] { DecodeOne(first); });
    if (second != first) {
        workers_.emplace_back([this, second] { DecodeOne(second); });
    }
}

std::optional<DecodedImage> Prefetcher::Take(const std::filesystem::path& path) {
    std::lock_guard lock(mutex_);
    const std::wstring key = std::filesystem::absolute(path).wstring();
    auto it = cache_.find(key);
    if (it == cache_.end()) {
        return std::nullopt;
    }
    DecodedImage image = std::move(it->second);
    cache_.erase(it);
    return image;
}

void Prefetcher::Stop() {
    {
        std::lock_guard lock(mutex_);
        stopped_ = true;
    }
    for (auto& worker : workers_) {
        if (worker.joinable()) {
            worker.join();
        }
    }
    workers_.clear();
}

void Prefetcher::DecodeOne(std::filesystem::path path) {
    try {
        WicDecoder decoder;
        DecodedImage image = decoder.Decode(path);
        std::lock_guard lock(mutex_);
        if (!stopped_) {
            cache_[std::filesystem::absolute(path).wstring()] = std::move(image);
        }
    } catch (...) {
    }
}

}  // namespace cpictures
```

- [ ] **Step 3: 接入 ViewerWindow**

Modify `src/viewer/viewer_window.h`:

```cpp
#include "image/prefetcher.h"

void WarmPrefetch();
Prefetcher prefetcher_;
```

Modify `LoadCurrentImage()`:

```cpp
    if (auto prefetched = prefetcher_.Take(imageList_.Current())) {
        decoded_ = std::move(*prefetched);
    } else {
        decoded_ = decoder_.Decode(imageList_.Current());
    }
```

At end of `LoadCurrentImage()` add:

```cpp
    WarmPrefetch();
```

Add method:

```cpp
void ViewerWindow::WarmPrefetch() {
    if (imageList_.Count() < 2) {
        return;
    }
    ImageList copy = imageList_;
    const auto next = copy.Next();
    copy = imageList_;
    const auto previous = copy.Previous();
    prefetcher_.Warm(previous, next);
}
```

Modify `CMakeLists.txt` target `cpictures` sources:

```cmake
  src/image/prefetcher.cpp
```

- [ ] **Step 4: 构建并手动验证**

Run:

```powershell
cmake --build --preset debug
.\build\vs2022-x64\Debug\cpictures.exe C:\path\to\folder\1.jpg
```

Expected:

```text
连续滚轮切换同目录多张图片；程序不崩溃；相邻图片切换比首次打开更快。
```

- [ ] **Step 5: 提交**

Run:

```powershell
git add src/image/prefetcher.* src/viewer CMakeLists.txt
git commit -m "feat: prefetch adjacent images"
```

## Task 10: 扩展插件 ABI 和本地插件管理

**Files:**
- Create: `src/codecs/cpictures_codec.h`
- Create: `src/codecs/codec_manager.h`
- Create: `src/codecs/codec_manager.cpp`
- Create: `tests/codec_manager_tests.cpp`
- Modify: `CMakeLists.txt`

**Interfaces:**
- Produces: C ABI `cpictures_codec_info`、`cpictures_probe`、`cpictures_decode`、`cpictures_free_image`。
- Produces: `CodecManager::FindPluginForExtension(ext)`。

- [ ] **Step 1: 写 ABI 头文件**

Create `src/codecs/cpictures_codec.h`:

```cpp
#pragma once

#include <stdint.h>

#ifdef _WIN32
#define CPICTURES_CODEC_EXPORT __declspec(dllexport)
#else
#define CPICTURES_CODEC_EXPORT
#endif

typedef struct cpictures_codec_info {
    uint32_t abi_version;
    const wchar_t* name;
    const wchar_t* version;
    const wchar_t* extensions;
} cpictures_codec_info;

typedef struct cpictures_decoded_image {
    uint32_t width;
    uint32_t height;
    uint32_t stride;
    uint8_t* bgra;
    uint64_t bgra_size;
} cpictures_decoded_image;

extern "C" {
typedef int(__stdcall* cpictures_codec_info_fn)(cpictures_codec_info* out_info);
typedef int(__stdcall* cpictures_probe_fn)(const wchar_t* path);
typedef int(__stdcall* cpictures_decode_fn)(const wchar_t* path, cpictures_decoded_image* out_image);
typedef void(__stdcall* cpictures_free_image_fn)(cpictures_decoded_image* image);
}
```

- [ ] **Step 2: 写插件管理器**

Create `src/codecs/codec_manager.h`:

```cpp
#pragma once

#include <filesystem>
#include <optional>
#include <string>
#include <vector>

namespace cpictures {

struct CodecPluginInfo {
    std::filesystem::path path;
    std::wstring name;
    std::wstring version;
    std::vector<std::wstring> extensions;
};

class CodecManager {
public:
    explicit CodecManager(std::filesystem::path codecsDirectory);
    std::vector<CodecPluginInfo> Discover() const;
    std::optional<CodecPluginInfo> FindPluginForExtension(std::wstring extension) const;

private:
    std::filesystem::path codecsDirectory_;
};

}  // namespace cpictures
```

Create `src/codecs/codec_manager.cpp`:

```cpp
#include "codecs/codec_manager.h"

#include <algorithm>
#include <windows.h>

#include "codecs/cpictures_codec.h"
#include "cpictures/supported_formats.h"

namespace cpictures {
namespace {
std::vector<std::wstring> SplitExtensions(const wchar_t* value) {
    std::vector<std::wstring> result;
    std::wstring current;
    for (const wchar_t* p = value; p && *p; ++p) {
        if (*p == L';') {
            if (!current.empty()) {
                result.push_back(NormalizeExtension(current));
                current.clear();
            }
        } else {
            current.push_back(*p);
        }
    }
    if (!current.empty()) {
        result.push_back(NormalizeExtension(current));
    }
    return result;
}
}

CodecManager::CodecManager(std::filesystem::path codecsDirectory)
    : codecsDirectory_(std::move(codecsDirectory)) {}

std::vector<CodecPluginInfo> CodecManager::Discover() const {
    std::vector<CodecPluginInfo> plugins;
    if (!std::filesystem::exists(codecsDirectory_)) {
        return plugins;
    }
    for (const auto& entry : std::filesystem::directory_iterator(codecsDirectory_)) {
        if (!entry.is_regular_file() || entry.path().extension() != L".dll") {
            continue;
        }
        HMODULE module = LoadLibraryW(entry.path().c_str());
        if (!module) {
            continue;
        }
        auto infoFn = reinterpret_cast<cpictures_codec_info_fn>(
            GetProcAddress(module, "cpictures_codec_info"));
        if (infoFn) {
            cpictures_codec_info info{};
            if (infoFn(&info) == 0 && info.abi_version == 1) {
                plugins.push_back({entry.path(), info.name ? info.name : L"",
                                   info.version ? info.version : L"",
                                   SplitExtensions(info.extensions)});
            }
        }
        FreeLibrary(module);
    }
    return plugins;
}

std::optional<CodecPluginInfo> CodecManager::FindPluginForExtension(std::wstring extension) const {
    extension = NormalizeExtension(extension);
    for (const auto& plugin : Discover()) {
        if (std::find(plugin.extensions.begin(), plugin.extensions.end(), extension) != plugin.extensions.end()) {
            return plugin;
        }
    }
    return std::nullopt;
}

}  // namespace cpictures
```

- [ ] **Step 3: 写发现测试**

Create `tests/codec_manager_tests.cpp`:

```cpp
#include <cstdlib>
#include <filesystem>
#include <iostream>

#include "codecs/codec_manager.h"

namespace {
void Expect(bool condition, const char* message) {
    if (!condition) {
        std::cerr << "FAIL: " << message << "\n";
        std::exit(1);
    }
}
}

int main() {
    cpictures::CodecManager manager(std::filesystem::temp_directory_path() / L"cpictures_missing_codecs");
    Expect(manager.Discover().empty(), "missing codecs directory yields no plugins");
    Expect(!manager.FindPluginForExtension(L".psd").has_value(), "missing plugin returns no match");
    std::cout << "codec manager tests passed\n";
    return 0;
}
```

Modify `CMakeLists.txt`:

```cmake
  add_executable(cpictures_codec_manager_tests tests/codec_manager_tests.cpp src/codecs/codec_manager.cpp)
  target_compile_features(cpictures_codec_manager_tests PRIVATE cxx_std_20)
  target_include_directories(cpictures_codec_manager_tests PRIVATE include src)
  target_compile_definitions(cpictures_codec_manager_tests PRIVATE UNICODE _UNICODE NOMINMAX WIN32_LEAN_AND_MEAN)
  target_link_libraries(cpictures_codec_manager_tests PRIVATE cpictures_core)
  add_test(NAME cpictures_codec_manager_tests COMMAND cpictures_codec_manager_tests)
```

- [ ] **Step 4: 运行测试**

Run:

```powershell
cmake --build --preset debug
ctest --preset debug
```

Expected:

```text
100% tests passed
```

- [ ] **Step 5: 提交**

Run:

```powershell
git add src/codecs tests/codec_manager_tests.cpp CMakeLists.txt
git commit -m "feat: add codec plugin discovery"
```

## Task 11: 扩展组件 manifest、SHA-256 校验、更新入口

**Files:**
- Create: `src/update/update_manifest.h`
- Create: `src/update/update_manifest.cpp`
- Create: `src/update/update_service.h`
- Create: `src/update/update_service.cpp`
- Create: `tests/update_manifest_tests.cpp`
- Modify: `src/viewer/viewer_window.cpp`
- Modify: `CMakeLists.txt`

**Interfaces:**
- Produces: `CodecPackageManifest`。
- Produces: `ParseManifest(std::wstring jsonText)`，先支持固定 manifest schema。
- Produces: `ComputeSha256(path) -> std::wstring`。
- Produces: 右键入口显示格式支持更新对话框。

- [ ] **Step 1: 写 manifest 类型和解析测试**

Create `tests/update_manifest_tests.cpp`:

```cpp
#include <cstdlib>
#include <iostream>

#include "update/update_manifest.h"

namespace {
void Expect(bool condition, const char* message) {
    if (!condition) {
        std::cerr << "FAIL: " << message << "\n";
        std::exit(1);
    }
}
}

int main() {
    const std::wstring json =
        LR"({"version":"1","packages":[{"id":"raw","name":"RAW 格式支持","version":"1.0.0","sha256":"ABCDEF","size":123,"url":"https://example.invalid/raw.zip","extensions":[".cr3",".nef"]}]})";
    const cpictures::CodecPackageManifest manifest = cpictures::ParseManifest(json);
    Expect(manifest.version == L"1", "manifest version");
    Expect(manifest.packages.size() == 1, "package count");
    Expect(manifest.packages[0].id == L"raw", "package id");
    Expect(manifest.packages[0].extensions[1] == L".nef", "package extension");
    std::cout << "update manifest tests passed\n";
    return 0;
}
```

- [ ] **Step 2: 写 manifest 解析器**

Create `src/update/update_manifest.h`:

```cpp
#pragma once

#include <string>
#include <vector>

namespace cpictures {

struct CodecPackage {
    std::wstring id;
    std::wstring name;
    std::wstring version;
    std::wstring sha256;
    unsigned long long size = 0;
    std::wstring url;
    std::vector<std::wstring> extensions;
};

struct CodecPackageManifest {
    std::wstring version;
    std::vector<CodecPackage> packages;
};

CodecPackageManifest ParseManifest(const std::wstring& jsonText);

}  // namespace cpictures
```

Create `src/update/update_manifest.cpp`:

```cpp
#include "update/update_manifest.h"

#include <regex>

namespace cpictures {
namespace {
std::wstring MatchOne(const std::wstring& text, const std::wregex& regex) {
    std::wsmatch match;
    if (std::regex_search(text, match, regex) && match.size() > 1) {
        return match[1].str();
    }
    return L"";
}

std::vector<std::wstring> MatchExtensions(const std::wstring& text) {
    std::vector<std::wstring> extensions;
    std::wsmatch match;
    if (!std::regex_search(text, match, std::wregex(LR"("extensions"\s*:\s*\[([^\]]*)\])"))) {
        return extensions;
    }
    const std::wstring body = match[1].str();
    std::wregex itemRegex(LR"("([^"]+)")");
    for (auto it = std::wsregex_iterator(body.begin(), body.end(), itemRegex);
         it != std::wsregex_iterator(); ++it) {
        extensions.push_back((*it)[1].str());
    }
    return extensions;
}
}

CodecPackageManifest ParseManifest(const std::wstring& jsonText) {
    CodecPackageManifest manifest;
    manifest.version = MatchOne(jsonText, std::wregex(LR"("version"\s*:\s*"([^"]+)")"));

    std::wregex packageRegex(LR"(\{("id"[^{}]+)\})");
    for (auto it = std::wsregex_iterator(jsonText.begin(), jsonText.end(), packageRegex);
         it != std::wsregex_iterator(); ++it) {
        const std::wstring body = (*it)[1].str();
        CodecPackage package;
        package.id = MatchOne(body, std::wregex(LR"("id"\s*:\s*"([^"]+)")"));
        package.name = MatchOne(body, std::wregex(LR"("name"\s*:\s*"([^"]+)")"));
        package.version = MatchOne(body, std::wregex(LR"("version"\s*:\s*"([^"]+)")"));
        package.sha256 = MatchOne(body, std::wregex(LR"("sha256"\s*:\s*"([^"]+)")"));
        package.url = MatchOne(body, std::wregex(LR"("url"\s*:\s*"([^"]+)")"));
        const std::wstring sizeText = MatchOne(body, std::wregex(LR"("size"\s*:\s*([0-9]+))"));
        package.size = sizeText.empty() ? 0 : std::stoull(sizeText);
        package.extensions = MatchExtensions(body);
        if (!package.id.empty()) {
            manifest.packages.push_back(std::move(package));
        }
    }
    return manifest;
}

}  // namespace cpictures
```

- [ ] **Step 3: 写更新服务第一阶段实现**

Create `src/update/update_service.h`:

```cpp
#pragma once

#include <filesystem>
#include <string>
#include <windows.h>

namespace cpictures {

std::wstring ComputeSha256(const std::filesystem::path& path);
void ShowFormatSupportDialog(HWND owner);

}  // namespace cpictures
```

Create `src/update/update_service.cpp`:

```cpp
#include "update/update_service.h"

#include <bcrypt.h>
#include <fstream>
#include <iomanip>
#include <iterator>
#include <sstream>
#include <vector>

namespace cpictures {

std::wstring ComputeSha256(const std::filesystem::path& path) {
    std::ifstream input(path, std::ios::binary);
    if (!input) {
        return L"";
    }
    std::vector<unsigned char> data((std::istreambuf_iterator<char>(input)), {});
    unsigned char hash[32]{};
    BCRYPT_ALG_HANDLE algorithm = nullptr;
    if (BCryptOpenAlgorithmProvider(&algorithm, BCRYPT_SHA256_ALGORITHM, nullptr, 0) < 0) {
        return L"";
    }
    const NTSTATUS status = BCryptHash(algorithm, nullptr, 0, data.data(),
                                       static_cast<ULONG>(data.size()), hash, sizeof(hash));
    BCryptCloseAlgorithmProvider(algorithm, 0);
    if (status < 0) {
        return L"";
    }
    std::wstringstream out;
    out << std::hex << std::setfill(L'0');
    for (unsigned char byte : hash) {
        out << std::setw(2) << static_cast<int>(byte);
    }
    return out.str();
}

void ShowFormatSupportDialog(HWND owner) {
    MessageBoxW(owner,
                L"格式支持组件将按需下载，下载前会显示组件名、版本、大小和来源，并校验 SHA-256。",
                L"安装/更新格式支持",
                MB_OK | MB_ICONINFORMATION);
}

}  // namespace cpictures
```

Modify `cpictures` link libraries:

```cmake
target_link_libraries(cpictures PRIVATE cpictures_core windowscodecs d2d1 dwrite dxgi ole32 shell32 user32 gdi32 bcrypt)
```

- [ ] **Step 4: 接入右键入口**

Modify `src/viewer/viewer_window.cpp` include:

```cpp
#include "update/update_service.h"
```

Replace `InstallOrUpdateFormats` branch:

```cpp
    case Command::InstallOrUpdateFormats:
        ShowFormatSupportDialog(hwnd_);
        break;
```

Modify `CMakeLists.txt`:

```cmake
  src/update/update_manifest.cpp
  src/update/update_service.cpp
```

and test:

```cmake
  add_executable(cpictures_update_manifest_tests tests/update_manifest_tests.cpp src/update/update_manifest.cpp)
  target_compile_features(cpictures_update_manifest_tests PRIVATE cxx_std_20)
  target_include_directories(cpictures_update_manifest_tests PRIVATE include src)
  target_compile_definitions(cpictures_update_manifest_tests PRIVATE UNICODE _UNICODE NOMINMAX WIN32_LEAN_AND_MEAN)
  add_test(NAME cpictures_update_manifest_tests COMMAND cpictures_update_manifest_tests)
```

- [ ] **Step 5: 运行测试**

Run:

```powershell
cmake --build --preset debug
ctest --preset debug
```

Expected:

```text
100% tests passed
```

- [ ] **Step 6: 提交**

Run:

```powershell
git add src/update tests/update_manifest_tests.cpp src/viewer CMakeLists.txt
git commit -m "feat: add format support update skeleton"
```

## Task 12: 安装器和 Win11 默认应用注册

**Files:**
- Create: `installer/wix/cpictures.wxs`
- Modify: `CMakeLists.txt`
- Modify: `README.md`

**Interfaces:**
- Produces: WiX MSI 源文件，注册应用路径、ProgID、SupportedTypes、Default Programs 能力。
- Produces: `cmake --install` 安装布局：`bin\cpictures.exe`、`codecs\`。

- [ ] **Step 1: 写安装布局**

Modify `CMakeLists.txt`:

```cmake
install(TARGETS cpictures RUNTIME DESTINATION bin)
install(DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}/docs" DESTINATION share/cpictures)
```

- [ ] **Step 2: 写 WiX 源文件**

Create `installer/wix/cpictures.wxs`:

```xml
<?xml version="1.0" encoding="UTF-8"?>
<Wix xmlns="http://wixtoolset.org/schemas/v4/wxs">
  <Package Name="cpictures" Manufacturer="cpictures" Version="0.1.0" UpgradeCode="0f79a0f5-865b-48c7-bcf3-328241f9eb08">
    <MajorUpgrade DowngradeErrorMessage="已安装更新版本的 cpictures。" />
    <MediaTemplate EmbedCab="yes" />

    <StandardDirectory Id="ProgramFilesFolder">
      <Directory Id="INSTALLFOLDER" Name="cpictures">
        <Directory Id="BinFolder" Name="bin" />
        <Directory Id="CodecsFolder" Name="codecs" />
      </Directory>
    </StandardDirectory>

    <ComponentGroup Id="ProductComponents">
      <Component Directory="BinFolder">
        <File Id="cpicturesExe" Source="$(var.SourceDir)\bin\cpictures.exe" KeyPath="yes" />
        <RegistryValue Root="HKLM" Key="Software\cpictures\Capabilities" Name="ApplicationName" Value="cpictures" Type="string" />
        <RegistryValue Root="HKLM" Key="Software\cpictures\Capabilities" Name="ApplicationDescription" Value="极简图片查看器" Type="string" />
        <RegistryValue Root="HKLM" Key="Software\cpictures\Capabilities\FileAssociations" Name=".jpg" Value="cpictures.image" Type="string" />
        <RegistryValue Root="HKLM" Key="Software\cpictures\Capabilities\FileAssociations" Name=".jpeg" Value="cpictures.image" Type="string" />
        <RegistryValue Root="HKLM" Key="Software\cpictures\Capabilities\FileAssociations" Name=".png" Value="cpictures.image" Type="string" />
        <RegistryValue Root="HKLM" Key="Software\cpictures\Capabilities\FileAssociations" Name=".bmp" Value="cpictures.image" Type="string" />
        <RegistryValue Root="HKLM" Key="Software\cpictures\Capabilities\FileAssociations" Name=".gif" Value="cpictures.image" Type="string" />
        <RegistryValue Root="HKLM" Key="Software\cpictures\Capabilities\FileAssociations" Name=".tif" Value="cpictures.image" Type="string" />
        <RegistryValue Root="HKLM" Key="Software\cpictures\Capabilities\FileAssociations" Name=".tiff" Value="cpictures.image" Type="string" />
        <RegistryValue Root="HKLM" Key="Software\cpictures\Capabilities\FileAssociations" Name=".webp" Value="cpictures.image" Type="string" />
        <RegistryValue Root="HKLM" Key="Software\cpictures\Capabilities\FileAssociations" Name=".heic" Value="cpictures.image" Type="string" />
        <RegistryValue Root="HKLM" Key="Software\cpictures\Capabilities\FileAssociations" Name=".heif" Value="cpictures.image" Type="string" />
        <RegistryValue Root="HKLM" Key="Software\RegisteredApplications" Name="cpictures" Value="Software\cpictures\Capabilities" Type="string" />
        <RegistryValue Root="HKCR" Key="cpictures.image" Value="cpictures 图片" Type="string" />
        <RegistryValue Root="HKCR" Key="cpictures.image\shell\open\command" Value="&quot;[BinFolder]cpictures.exe&quot; &quot;%1&quot;" Type="string" />
      </Component>
    </ComponentGroup>

    <Feature Id="Main" Title="cpictures" Level="1">
      <ComponentGroupRef Id="ProductComponents" />
    </Feature>
  </Package>
</Wix>
```

Note: `UpgradeCode` 已固定；后续版本保持同一个值，避免升级链断裂。

- [ ] **Step 3: README 增加安装说明**

Append to `README.md`:

```markdown
## 安装包

安装器使用 WiX Toolset v4。先生成安装布局：

```powershell
cmake --build --preset release
cmake --install build\vs2022-x64 --config Release --prefix dist
```

再用 WiX 编译 MSI。文件关联必须遵守 Win11 默认应用机制；安装后在系统“默认应用”中选择 cpictures。
```

- [ ] **Step 4: 验证安装布局**

Run:

```powershell
cmake --build --preset release
cmake --install build\vs2022-x64 --config Release --prefix dist
Test-Path dist\bin\cpictures.exe
```

Expected:

```text
True
```

- [ ] **Step 5: 提交**

Run:

```powershell
git add installer CMakeLists.txt README.md
git commit -m "chore: add installer skeleton"
```

## Task 13: 最终验证和缺口登记

**Files:**
- Modify: `README.md`
- Create: `docs/verification/2026-07-06-cpictures.md`

**Interfaces:**
- Produces: 验证记录，列出通过项、环境、已知限制。

- [ ] **Step 1: 创建验证目录和记录**

Create `docs/verification/2026-07-06-cpictures.md`:

```markdown
# cpictures 验证记录

日期：2026-07-06

## 环境

- Windows 11
- Visual Studio 2022 Build Tools
- Windows SDK
- CMake 3.24+

## 命令

```powershell
cmake --preset vs2022-x64
cmake --build --preset debug
ctest --preset debug
cmake --build --preset release
```

## 手动验证

- [ ] 打开 JPEG/PNG/BMP/TIFF/GIF 静态图。
- [ ] 无标题栏、无边框、无工具条。
- [ ] 默认 100% 原图；大图不超过屏幕。
- [ ] `Esc` 关闭。
- [ ] 单击切换左下角角标。
- [ ] 滚轮切换上一张/下一张。
- [ ] `Ctrl + 滚轮` 缩放。
- [ ] `1` 实际大小。
- [ ] `0` 适应屏幕。
- [ ] `F11` 全屏。
- [ ] `Ctrl+L`、`Ctrl+R` 临时旋转且不修改原文件。
- [ ] 右键菜单只含指定项目。
- [ ] `Ctrl+C` 复制文件后可粘贴到资源管理器。
- [ ] `Ctrl+Shift+C` 复制完整路径。

## 已知限制

- 扩展格式下载 UI 已有确认说明和 hash 校验函数；生产发布需要接入 HTTPS 下载源和组件包。
- WiX 默认应用注册需要在安装测试机验证所有 Capabilities\FileAssociations。
- RAW/PSD/EXR/HDR 解码组件需要按插件 ABI 独立实现和签名发布。
```

- [ ] **Step 2: 执行自动验证**

Run:

```powershell
cmake --preset vs2022-x64
cmake --build --preset debug
ctest --preset debug
cmake --build --preset release
```

Expected:

```text
100% tests passed
```

- [ ] **Step 3: 执行手动验证并勾选记录**

Open `docs/verification/2026-07-06-cpictures.md` and mark each verified checkbox with `[x]`.

- [ ] **Step 4: 提交**

Run:

```powershell
git add README.md docs/verification/2026-07-06-cpictures.md
git commit -m "test: record cpictures verification"
```

## 自检

Spec 覆盖：
- 极简无 chrome 窗口：Task 5、Task 6。
- 默认 100% 原图贴合：Task 2、Task 6。
- 同目录图片列表、滚轮切图：Task 3、Task 7。
- 缩放、全屏、临时旋转：Task 7。
- 左下角角标：Task 2、Task 6。
- 右键菜单、复制文件、复制路径：Task 8。
- 核心格式 WIC：Task 4。
- 扩展插件和更新入口：Task 10、Task 11。
- Win11 默认应用注册：Task 12。
- 测试和验证：Task 1、Task 2、Task 3、Task 4、Task 10、Task 11、Task 13。

执行提醒：
- Task 11 中 manifest 解析器是固定 schema 轻量解析；生产版可替换为小型 JSON 库，但不应让 JSON 库进入启动热路径。
- Task 12 WiX 文件是第一阶段安装器，已固定 UpgradeCode；发布前在干净 Win11 测试机验证默认应用注册。
- 扩展 codec 实现应另起计划：`cpictures-codec-pack`，按 AVIF、SVG、PSD/PSB、RAW、EXR/HDR 分插件做独立构建和签名。
