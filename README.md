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
