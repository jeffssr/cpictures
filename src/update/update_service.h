#pragma once

#include <filesystem>
#include <string>
#include <string_view>
#include <windows.h>

namespace cpictures {

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
std::wstring ComputeSha256(const std::filesystem::path& path);
bool VerifySha256Digest(const std::filesystem::path& path, const std::wstring& digest);
void ShowFormatSupportDialog(HWND owner);
void CheckForUpdates(HWND owner);

}  // namespace cpictures
