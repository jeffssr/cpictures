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
