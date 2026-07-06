# cpictures 验证记录

日期：2026-07-06

基线提交：`dbf897f42c7be6ff26ee37ef5150b50ec5673ef7`

## 环境

- 工作区：`D:\Jimmy\ai_docs\cpictures`
- 操作系统：Windows 11
- 工具链：Visual Studio Community 2026 Developer Shell（`amd64`）
- 构建系统：CMake 4.2+，MSBuild 18.7.8

## 自动验证

执行命令：

```powershell
& 'C:\Program Files\Microsoft Visual Studio\18\Community\Common7\Tools\Launch-VsDevShell.ps1' -Arch amd64 -HostArch amd64 | Out-Null
cmake --preset vs2026-x64
cmake --build --preset debug -- /m:1
ctest --preset debug
cmake --build --preset release -- /m:1
```

实际结果：

- `cmake --preset vs2026-x64`：成功，生成目录 `build/vs2026-x64`
- `cmake --build --preset debug -- /m:1`：成功，生成 `Debug\cpictures.exe` 及全部测试二进制
- `ctest --preset debug`：成功，`100% tests passed, 0 tests failed out of 6`
- `cmake --build --preset release -- /m:1`：成功，生成 `Release\cpictures.exe` 及全部测试二进制

测试明细：

1. `cpictures_core_tests`：Passed
2. `cpictures_wic_tests`：Passed
3. `cpictures_platform_tests`：Passed
4. `cpictures_codec_manager_tests`：Passed
5. `cpictures_codec_abi_c_test`：Passed
6. `cpictures_update_manifest_tests`：Passed

总测试时间：`0.51 sec`

## 手动验证清单

- [ ] 打开 JPEG/PNG/BMP/TIFF/GIF 静态图
- [ ] 无标题栏、无边框、无工具条
- [ ] 默认 100% 原图；大图不超过屏幕
- [ ] `Esc` 关闭
- [ ] 单击切换左下角角标
- [ ] 滚轮切换上一张/下一张
- [ ] `Ctrl + 滚轮` 缩放
- [ ] `1` 实际大小
- [ ] `0` 适应屏幕
- [ ] `F11` 全屏
- [ ] `Ctrl+L`、`Ctrl+R` 临时旋转且不修改原文件
- [ ] 右键菜单只含指定项目
- [ ] `Ctrl+C` 复制文件后可粘贴到资源管理器
- [ ] `Ctrl+Shift+C` 复制完整路径

## 未完成手动验证

本环境 GUI smoke 的 SendKeys / 窗口焦点不稳定，无法把界面交互与剪贴板体验项作为可靠自动化证据。因此本次仅记录自动构建与测试结果，所有 GUI、视觉、输入、剪贴板相关项保持未勾选，待人工逐项确认。

## 已知限制

- 扩展下载源/组件包尚未接入；当前只有确认说明与 hash 校验相关能力，生产发布仍需补齐 HTTPS 下载源和组件包分发链路。
- WiX MSI 未在本次验证中实际编译，也未做安装/卸载与默认应用注册联调测试。
- RAW/PSD/EXR/HDR 插件需要按既定插件 ABI 独立实现、签名并单独交付。
- GUI 视觉表现、窗口交互与剪贴板体验仍需人工验证，当前环境无法给出稳定自动化结论。
