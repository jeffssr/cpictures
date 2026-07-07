# cpictures 在线升级设计

## 目标

在图片查看窗口右键菜单最底部新增“检查更新...”。用户点击后，cpictures 从 GitHub Release 检查新版，发现新版时下载 `.msi` 安装包并交给 Windows Installer 执行升级。升级后不主动改动用户已经设置好的默认图片打开应用。

## 范围

- 手动触发检查更新，不做后台自动检查。
- 下载 GitHub Release 中的 `.msi` 安装包，不做 `.exe` 安装器。
- 使用已有 WiX `UpgradeCode` 做覆盖升级。
- 不静默安装，安装过程由 Windows Installer UI 和 UAC 处理。
- 不修改用户的默认应用设置，不重新写入 UserChoice。

## 用户入口

右键菜单结构保持现有极简风格：

1. 复制
2. 复制路径
3. 分隔线
4. 实际大小
5. 适应屏幕
6. 全屏/取消全屏
7. 分隔线
8. 安装/更新格式支持...
9. 检查更新...

“检查更新...”放在最底部。

## 更新源

客户端请求：

```text
https://api.github.com/repos/jeffssr/cpictures/releases/latest
```

读取字段：

- `tag_name`：最新版本标签，例如 `v0.2.0`。
- `assets[].name`：筛选 `.msi`，优先匹配 `cpictures-*.msi`。
- `assets[].browser_download_url`：安装包下载地址。

版本比较规则：

- 本地版本来自 CMake 项目版本和 Windows 资源版本。
- 远端版本移除前缀 `v` 后按数字段比较。
- `1.10.0` 大于 `1.2.0`。
- 远端版本小于或等于本地版本时提示已是最新版本。

## 下载与安装

下载目录：

```text
%TEMP%\cpictures-update\
```

下载文件名使用 release asset 原文件名。

下载完成后启动：

```text
msiexec.exe /i "<downloaded-msi-path>"
```

客户端启动安装器后不等待安装完成。Windows Installer 负责关闭旧文件、覆盖安装、注册表组件升级。因为 WiX 包使用同一个 `UpgradeCode`，升级过程保留产品身份；Windows 默认应用选择由系统管理，不由 cpictures 覆盖。

## UI 行为

- 检查期间显示普通 MessageBox 提示，不在主窗口增加常驻 UI。
- 有新版时提示版本号并询问是否下载升级。
- 无新版时提示“已是最新版本。”。
- 下载失败、解析失败、找不到 MSI、安装器启动失败时显示简短错误。

## 组件边界

- `context_menu`：新增菜单项并返回新命令。
- `commands`：新增 `CheckForUpdates`。
- `viewer_window`：接收命令并调用更新服务。
- `update_service`：负责版本比较、GitHub release 解析、下载、启动安装器。
- `tests/update_manifest_tests.cpp` 或新测试文件：覆盖版本比较和 release asset 选择，不访问网络。

## 测试策略

- 单元测试版本比较：
  - `v0.2.0` 大于 `0.1.0`。
  - `1.10.0` 大于 `1.2.0`。
  - `0.1.0` 不大于 `0.1.0`。
- 单元测试 release asset 筛选：
  - 存在 `cpictures-0.2.0-x64.msi` 时选中。
  - 只有 `.zip` 时返回空。
- 构建测试：
  - Debug 全量 `ctest`。
  - Release `cpictures` 构建。
- 手动 smoke：
  - 右键菜单能看到“检查更新...”。
  - 在无网络或不可用 release 情况下显示错误，不崩溃。

## 发布要求

以后发布版本时，GitHub Release 必须上传 `.msi` asset，文件名建议：

```text
cpictures-<version>-x64.msi
```

例如：

```text
cpictures-0.2.0-x64.msi
```

