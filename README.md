# cpictures

Win11 原生图片查看器。目标：极速、轻量、极简。

## 构建前提

- Visual Studio Community 2026 或 Build Tools 2026，包含 Desktop development with C++
- Windows SDK
- CMake 4.2+

若普通 PowerShell 找不到 `cl` 或 `cmake`，使用 “Developer PowerShell for VS 2026”。

## 构建

```powershell
cmake --preset vs2026-x64
cmake --build --preset debug
ctest --preset debug
```

## 运行

```powershell
.\build\vs2026-x64\Debug\cpictures.exe C:\path\to\image.jpg
```

## 安装包

安装布局由 CMake `install()` 生成，包含 `bin\cpictures.exe`、`codecs\` 目录和 `share\cpictures\docs`。

WiX 源文件位于 `installer/wix/cpictures.wxs`，使用 WiX Toolset v4 schema。它只注册应用能力（Capabilities）和文件关联信息，供 Windows 11 的“默认应用”界面识别；安装后仍需在系统“默认应用”里手动选择 cpictures，不能静默抢占默认应用。

生成安装布局：

```powershell
cmake --build --preset release
cmake --install build\vs2026-x64 --config Release --prefix dist
```

## 验证记录

最新验证记录见 [docs/verification/2026-07-06-cpictures.md](/D:/Jimmy/ai_docs/cpictures/docs/verification/2026-07-06-cpictures.md)。其中区分了已完成自动验证、未完成手动验证，以及当前已知限制。
