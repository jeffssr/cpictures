# Task 5 Report

## Summary

已完成 cpictures 的 Win32 主程序入口和无 chrome 窗口骨架。当前仅包含入口、消息循环、窗口类注册、无边框 `WS_POPUP` 窗口、ESC 关闭，以及最小的图片路径加载/校验。未加入 Task 6 之后的渲染、缩放、切图、右键菜单或叠层逻辑。

## Files changed

- `D:\Jimmy\ai_docs\cpictures\src\main.cpp`
- `D:\Jimmy\ai_docs\cpictures\src\app\app.h`
- `D:\Jimmy\ai_docs\cpictures\src\app\app.cpp`
- `D:\Jimmy\ai_docs\cpictures\src\viewer\viewer_window.h`
- `D:\Jimmy\ai_docs\cpictures\src\viewer\viewer_window.cpp`
- `D:\Jimmy\ai_docs\cpictures\CMakeLists.txt`
- `D:\Jimmy\ai_docs\cpictures\.superpowers\sdd\progress.md`

## Verification commands and exact outcome

1. `& 'C:\Program Files\Microsoft Visual Studio\18\Community\Common7\Tools\Launch-VsDevShell.ps1' -Arch amd64 -HostArch amd64 | Out-Null; cmake --build --preset debug`
   - 首次失败: MSBuild `MultiToolTask` 触发瞬时文件锁，重跑前一次未完成的 tlog 写入。
   - 重跑成功: `cpictures.exe`、`cpictures_core_tests.exe`、`cpictures_wic_tests.exe` 全部生成。
2. `& 'C:\Program Files\Microsoft Visual Studio\18\Community\Common7\Tools\Launch-VsDevShell.ps1' -Arch amd64 -HostArch amd64 | Out-Null; ctest --preset debug`
   - 通过: `2/2` tests passed.

## Smoke test outcome

运行 `D:\Jimmy\ai_docs\cpictures\build\vs2026-x64\Debug\cpictures.exe` 无参数。
结果: 程序进入缺参分支，未留下后台会话；为避免消息框阻塞，2 秒后确认进程仍存活并强制结束。

## Concerns

- 当前窗口只具备骨架和 ESC 关闭；未实现 Task 6 的 D2D 渲染、缩放和贴合逻辑。
- 源文件最初出现过 MSVC 代码页告警，已改为 ASCII 文本消息；后续新增中文文本时要注意编码。

## Review follow-up

- 已补入口级异常兜底：`RunApplication()` 现在捕获启动期 `std::exception` 与未知异常，统一弹出 ASCII 错误框并返回非零。
- 已补稳定背景填充：`WM_PAINT` 现在显式填充 `COLOR_WINDOW`，避免 Task 6 渲染前留下脏客户端区。
