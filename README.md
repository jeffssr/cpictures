# cpictures

Win11 原生图片查看器。目标是超快、轻量、极简：打开窗口只显示图片本身，常用操作放在鼠标和键盘习惯里。

## 功能

- 支持常见图片格式：JPG、PNG、BMP、GIF、TIFF、WebP、HEIC、HEIF、ICO 等。
- 默认 100% 原图显示，窗口按图片尺寸自动贴合，最大不超过屏幕。
- 支持缩放、滚轮浏览、前后切换、拖动窗口、右键菜单、全屏/取消全屏。
- `Esc` 关闭窗口。
- 单击图片可切换左下角信息：`n/m  文件名.格式`。
- 可注册为 Windows 默认图片查看器。
- 软件图标和图片文件关联图标分离：`cpictures.exe` 使用应用图标，图片文件使用 `cpictures-file.ico`。

## 目录

- `src/`：应用源码。
- `include/`：公共头文件。
- `tests/`：自动化测试源码。
- `assets/icon/`：应用图标、文件关联图标及生成脚本。
- `installer/wix/`：WiX MSI 安装包定义。
- `docs/`：设计、计划和验证记录。

`build/`、`dist/`、`release/` 是本地生成产物，不提交源码仓。

## 构建环境

- Windows 11
- Visual Studio Community 2026 或 Build Tools 2026
- Windows SDK
- CMake 4.2+
- .NET SDK，用于安装 WiX CLI

若普通 PowerShell 找不到 `cl` 或 `cmake`，使用 “Developer PowerShell for VS 2026”。

## 源码构建

```powershell
cmake --preset vs2026-x64
cmake --build --preset debug
ctest --preset debug
```

运行调试版：

```powershell
.\build\vs2026-x64\Debug\cpictures.exe C:\path\to\image.jpg
```

## 生成 EXE

```powershell
cmake --build --preset release --target cpictures
cmake --install build\vs2026-x64 --config Release
```

生成结果：

- `dist\bin\cpictures.exe`
- `dist\bin\cpictures-file.ico`

## 生成 MSI

```powershell
dotnet tool install --tool-path build\tools-wix4 wix --version 4.*
.\build\tools-wix4\wix.exe build -arch x64 installer\wix\cpictures.wxs -d SourceDir=dist -o release\cpictures-x64.msi
.\build\tools-wix4\wix.exe msi validate release\cpictures-x64.msi
```

必须使用 `-arch x64`。不带该参数会生成 32 位 MSI。

## 安装和默认应用

安装 `cpictures-x64.msi` 后，Windows 会注册 cpictures 的默认应用能力和 `cpictures.image` 文件类型。

Windows 11 不允许安装包静默抢占默认应用。安装后需要手动设置：

1. 打开“设置”。
2. 进入“应用” -> “默认应用”。
3. 找到 cpictures。
4. 为需要的图片格式选择 cpictures。

图片文件前的图标来自 `cpictures-file.ico`。若资源管理器仍显示旧图标，重启“Windows 资源管理器”或注销后再看。

## 卸载

在“设置” -> “应用” -> “已安装的应用”中卸载 cpictures。卸载后安装目录和注册表项由 MSI 清理。

## 发布产物

Release 建议包含：

- `cpictures-x64.msi`：安装包。
- `cpictures-portable-x64.zip`：便携版，包含 `cpictures.exe` 和 `cpictures-file.ico`。

便携版不会注册默认应用，只适合手动运行或测试。
