# 在线升级 Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** 给 cpictures 右键菜单新增“检查更新...”，从 GitHub Release 下载新版 `.msi` 并启动 Windows Installer 升级。

**Architecture:** 更新逻辑集中在 `src/update/update_service.*`：纯函数负责版本比较和 GitHub release JSON 解析，可单元测试；WinHTTP 下载和 `msiexec` 启动留在 UI 命令路径。菜单层只新增命令映射，Viewer 只转发命令。

**Tech Stack:** C++20、Win32、WinHTTP、ShellExecuteW、GitHub REST API、现有 CMake/CTest。

## Global Constraints

- 手动触发检查更新，不做后台自动检查。
- 下载 GitHub Release 中的 `.msi` 安装包，不做 `.exe` 安装器。
- 使用已有 WiX `UpgradeCode` 做覆盖升级。
- 不静默安装，安装过程由 Windows Installer UI 和 UAC 处理。
- 不修改用户的默认应用设置，不重新写入 UserChoice。
- 不引入第三方 JSON 或网络依赖，沿用项目现有轻量 Win32/C++ 风格。

---

### Task 1: 更新元数据纯逻辑

**Files:**
- Modify: `src/update/update_service.h`
- Modify: `src/update/update_service.cpp`
- Modify: `tests/update_manifest_tests.cpp`
- Modify: `CMakeLists.txt`

**Interfaces:**
- Produces: `struct ReleaseAsset { std::wstring name; std::wstring downloadUrl; std::wstring digest; unsigned long long size = 0; };`
- Produces: `struct ReleaseInfo { std::wstring tagName; ReleaseAsset installer; };`
- Produces: `int CompareVersions(std::wstring_view left, std::wstring_view right);`
- Produces: `bool IsNewerVersion(std::wstring_view remote, std::wstring_view local);`
- Produces: `ReleaseInfo ParseLatestRelease(const std::wstring& jsonText);`
- Consumes: existing hand-written JSON parsing style in `src/update/update_manifest.cpp`.

- [ ] **Step 1: Write failing tests**

Add to `tests/update_manifest_tests.cpp`:

```cpp
void TestVersionComparison() {
    Expect(cpictures::CompareVersions(L"v0.2.0", L"0.1.0") > 0, "v0.2.0 newer than 0.1.0");
    Expect(cpictures::CompareVersions(L"1.10.0", L"1.2.0") > 0, "numeric version comparison");
    Expect(cpictures::CompareVersions(L"0.1.0", L"0.1.0") == 0, "same versions compare equal");
    Expect(!cpictures::IsNewerVersion(L"0.1.0", L"0.1.0"), "same version is not newer");
}

void TestLatestReleaseSelectsMsiAsset() {
    const std::wstring json =
        LR"({"tag_name":"v0.2.0","assets":[{"name":"cpictures-0.2.0-x64.zip","browser_download_url":"https://example.invalid/cpictures.zip"},{"name":"cpictures-0.2.0-x64.msi","browser_download_url":"https://example.invalid/cpictures.msi","digest":"sha256:abc","size":456}]})";
    const cpictures::ReleaseInfo release = cpictures::ParseLatestRelease(json);
    Expect(release.tagName == L"v0.2.0", "release tag name");
    Expect(release.installer.name == L"cpictures-0.2.0-x64.msi", "release installer name");
    Expect(release.installer.downloadUrl == L"https://example.invalid/cpictures.msi", "release installer url");
    Expect(release.installer.digest == L"sha256:abc", "release installer digest");
    Expect(release.installer.size == 456, "release installer size");
}

void TestLatestReleaseWithoutMsiReturnsEmptyInstaller() {
    const cpictures::ReleaseInfo release = cpictures::ParseLatestRelease(
        LR"({"tag_name":"v0.2.0","assets":[{"name":"cpictures-0.2.0-x64.zip","browser_download_url":"https://example.invalid/cpictures.zip"}]})");
    Expect(release.tagName == L"v0.2.0", "release without msi keeps tag");
    Expect(release.installer.name.empty(), "release without msi has no installer");
    Expect(release.installer.downloadUrl.empty(), "release without msi has no download url");
}
```

Call tests from `main()` before SHA tests:

```cpp
    TestVersionComparison();
    TestLatestReleaseSelectsMsiAsset();
    TestLatestReleaseWithoutMsiReturnsEmptyInstaller();
```

- [ ] **Step 2: Verify RED**

Run:

```powershell
& 'C:\Program Files\Microsoft Visual Studio\18\Community\Common7\Tools\Launch-VsDevShell.ps1' -Arch amd64 -HostArch amd64 | Out-Null
cmake --build --preset debug --target cpictures_update_manifest_tests -- /m:1
```

Expected: compile fails because `CompareVersions`, `IsNewerVersion`, `ReleaseInfo`, `ParseLatestRelease` are missing.

- [ ] **Step 3: Implement minimal pure logic**

In `src/update/update_service.h`, add:

```cpp
#include <string_view>

struct ReleaseAsset {
    std::wstring name;
    std::wstring downloadUrl;
    std::wstring digest;
    unsigned long long size = 0;
};

struct ReleaseInfo {
    std::wstring tagName;
    ReleaseAsset installer;
};

int CompareVersions(std::wstring_view left, std::wstring_view right);
bool IsNewerVersion(std::wstring_view remote, std::wstring_view local);
ReleaseInfo ParseLatestRelease(const std::wstring& jsonText);
```

In `src/update/update_service.cpp`, implement:

```cpp
std::wstring_view TrimVersionPrefix(std::wstring_view value) {
    while (!value.empty() && iswspace(value.front()) != 0) {
        value.remove_prefix(1);
    }
    if (!value.empty() && (value.front() == L'v' || value.front() == L'V')) {
        value.remove_prefix(1);
    }
    return value;
}

unsigned long long ReadVersionPart(std::wstring_view value, size_t* pos) {
    unsigned long long result = 0;
    while (*pos < value.size() && value[*pos] >= L'0' && value[*pos] <= L'9') {
        const unsigned int digit = static_cast<unsigned int>(value[*pos] - L'0');
        constexpr unsigned long long kMaxValue = ~0ULL;
        if (result > (kMaxValue - digit) / 10) {
            return kMaxValue;
        }
        result = (result * 10) + digit;
        ++(*pos);
    }
    return result;
}

int CompareVersions(std::wstring_view left, std::wstring_view right) {
    left = TrimVersionPrefix(left);
    right = TrimVersionPrefix(right);
    size_t leftPos = 0;
    size_t rightPos = 0;
    while (leftPos < left.size() || rightPos < right.size()) {
        const unsigned long long leftPart = ReadVersionPart(left, &leftPos);
        const unsigned long long rightPart = ReadVersionPart(right, &rightPos);
        if (leftPart < rightPart) {
            return -1;
        }
        if (leftPart > rightPart) {
            return 1;
        }
        if (leftPos < left.size() && left[leftPos] == L'.') {
            ++leftPos;
        }
        if (rightPos < right.size() && right[rightPos] == L'.') {
            ++rightPos;
        }
        while (leftPos < left.size() && left[leftPos] != L'.' && !iswdigit(left[leftPos])) {
            ++leftPos;
        }
        while (rightPos < right.size() && right[rightPos] != L'.' && !iswdigit(right[rightPos])) {
            ++rightPos;
        }
    }
    return 0;
}

bool IsNewerVersion(std::wstring_view remote, std::wstring_view local) {
    return CompareVersions(remote, local) > 0;
}
```

Add simple JSON helpers scoped to `update_service.cpp` to parse root `tag_name`, `assets`, each asset object fields `name`, `browser_download_url`, `digest`, `size`; select first `.msi` whose name starts with `cpictures-` if present, otherwise first `.msi`.

- [ ] **Step 4: Verify GREEN**

Run:

```powershell
cmake --build --preset debug --target cpictures_update_manifest_tests -- /m:1
ctest --preset debug -R cpictures_update_manifest_tests --output-on-failure
```

Expected: `100% tests passed, 0 tests failed out of 1`.

- [ ] **Step 5: Commit**

```powershell
git add src/update/update_service.h src/update/update_service.cpp tests/update_manifest_tests.cpp CMakeLists.txt
git commit -m "feat(update): parse release metadata"
```

---

### Task 2: 下载与启动安装器

**Files:**
- Modify: `src/update/update_service.h`
- Modify: `src/update/update_service.cpp`
- Modify: `CMakeLists.txt`

**Interfaces:**
- Consumes: `ReleaseInfo ParseLatestRelease(const std::wstring& jsonText);`
- Produces: `void CheckForUpdates(HWND owner);`
- Produces internal helpers `std::wstring CurrentVersion();`, `std::optional<std::wstring> HttpGetText(const wchar_t* url);`, `bool DownloadFile(const std::wstring& url, const std::filesystem::path& path);`, `bool LaunchInstaller(const std::filesystem::path& path);`

- [ ] **Step 1: Write failing compile integration**

In `src/update/update_service.h`, declare:

```cpp
void CheckForUpdates(HWND owner);
```

Temporarily add call in `viewer_window.cpp` later in Task 3. For this task, build should fail only if implementation missing once linked through Task 3. No unit test for network; network paths tested by manual smoke.

- [ ] **Step 2: Implement WinHTTP helpers**

In `src/update/update_service.cpp`, include:

```cpp
#include <optional>
#include <shlwapi.h>
#include <winhttp.h>
```

Implement URL parsing using `WinHttpCrackUrl`, HTTPS session via `WinHttpOpen`, `WinHttpConnect`, `WinHttpOpenRequest`, `WinHttpSendRequest`, `WinHttpReceiveResponse`, read response with `WinHttpQueryDataAvailable` and `WinHttpReadData`.

Set headers:

```text
Accept: application/vnd.github+json
User-Agent: cpictures
```

Use `WINHTTP_FLAG_SECURE` for HTTPS. Reject non-200 status for metadata. For downloads, follow redirects by enabling `WINHTTP_OPTION_REDIRECT_POLICY` with `WINHTTP_OPTION_REDIRECT_POLICY_ALWAYS`.

- [ ] **Step 3: Implement CheckForUpdates UI flow**

Pseudo-code:

```cpp
void CheckForUpdates(HWND owner) {
    MessageBoxW(owner, L"正在检查更新...", L"cpictures", MB_OK | MB_ICONINFORMATION);
    const auto json = HttpGetText(L"https://api.github.com/repos/jeffssr/cpictures/releases/latest");
    if (!json) {
        MessageBoxW(owner, L"检查更新失败。", L"cpictures", MB_OK | MB_ICONERROR);
        return;
    }
    const ReleaseInfo release = ParseLatestRelease(*json);
    if (release.tagName.empty()) {
        MessageBoxW(owner, L"检查更新失败。", L"cpictures", MB_OK | MB_ICONERROR);
        return;
    }
    if (!IsNewerVersion(release.tagName, CurrentVersion())) {
        MessageBoxW(owner, L"已是最新版本。", L"cpictures", MB_OK | MB_ICONINFORMATION);
        return;
    }
    if (release.installer.downloadUrl.empty()) {
        MessageBoxW(owner, L"未找到安装包。", L"cpictures", MB_OK | MB_ICONERROR);
        return;
    }
    const int answer = MessageBoxW(owner, L"发现新版本，是否下载并安装？", L"cpictures", MB_YESNO | MB_ICONQUESTION);
    if (answer != IDYES) {
        return;
    }
    const auto downloadPath = UpdateDownloadPath(release.installer.name);
    if (!DownloadFile(release.installer.downloadUrl, downloadPath)) {
        MessageBoxW(owner, L"下载失败。", L"cpictures", MB_OK | MB_ICONERROR);
        return;
    }
    if (!LaunchInstaller(downloadPath)) {
        MessageBoxW(owner, L"启动安装失败。", L"cpictures", MB_OK | MB_ICONERROR);
        return;
    }
}
```

Use `ShellExecuteW(owner, L"open", L"msiexec.exe", parameters.c_str(), nullptr, SW_SHOWNORMAL)` with parameters `/i "<path>"`.

- [ ] **Step 4: Link WinHTTP**

In `CMakeLists.txt`, add `winhttp` to `target_link_libraries(cpictures ...)`.

- [ ] **Step 5: Build**

Run:

```powershell
cmake --build --preset debug --target cpictures -- /m:1
```

Expected: `cpictures.exe` builds.

- [ ] **Step 6: Commit**

```powershell
git add src/update/update_service.h src/update/update_service.cpp CMakeLists.txt
git commit -m "feat(update): download release installer"
```

---

### Task 3: 右键菜单接入

**Files:**
- Modify: `include/cpictures/commands.h`
- Modify: `src/platform/context_menu.cpp`
- Modify: `src/viewer/viewer_window.cpp`

**Interfaces:**
- Consumes: `void CheckForUpdates(HWND owner);`
- Produces: context menu command `Command::CheckForUpdates`.

- [ ] **Step 1: Write failing compile change**

In `include/cpictures/commands.h`, add enum value:

```cpp
    CheckForUpdates
```

In `src/platform/context_menu.cpp`, add ID:

```cpp
constexpr UINT kCheckForUpdates = 1009;
```

Append item after format support:

```cpp
    AppendMenuW(menu, MF_STRING, kCheckForUpdates, L"\x68C0\x67E5\x66F4\x65B0...");
```

Switch:

```cpp
    case kCheckForUpdates:
        return Command::CheckForUpdates;
```

In `src/viewer/viewer_window.cpp`, add `ExecuteCommand` case:

```cpp
    case Command::CheckForUpdates:
        CheckForUpdates(hwnd_);
        break;
```

- [ ] **Step 2: Build**

Run:

```powershell
cmake --build --preset debug --target cpictures -- /m:1
```

Expected: compile/link succeeds.

- [ ] **Step 3: Commit**

```powershell
git add include/cpictures/commands.h src/platform/context_menu.cpp src/viewer/viewer_window.cpp
git commit -m "feat(update): add update menu command"
```

---

### Task 4: 全量验证与 smoke

**Files:**
- No code edits expected.

**Interfaces:**
- Consumes: all earlier tasks.
- Produces: verified branch.

- [ ] **Step 1: Run full verification**

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

Release `cpictures.exe` builds.

- [ ] **Step 2: Manual right-click smoke**

Run Debug `cpictures.exe` against existing temp sample image:

```powershell
.\build\vs2026-x64\Debug\cpictures.exe "$env:TEMP\cpictures-single-image-repro\only.png"
```

Right-click image. Verify “检查更新...” appears below “安装/更新格式支持...”.

- [ ] **Step 3: Check git diff**

Run:

```powershell
git status --short --branch
git diff --check
```

Expected: no whitespace errors; only intended files modified after final commit.

---

## Self-Review

- Spec coverage: menu entry, GitHub latest release, `.msi` selection, version compare, download path, `msiexec /i`, no default-app rewrite, errors, tests all covered.
- Placeholder scan: no `TBD`, no `TODO`, no “implement later”.
- Type consistency: `ReleaseAsset`, `ReleaseInfo`, `CompareVersions`, `IsNewerVersion`, `ParseLatestRelease`, `CheckForUpdates` names match tasks.

