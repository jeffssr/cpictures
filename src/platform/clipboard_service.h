#pragma once

#include <filesystem>
#include <string>

#include <windows.h>

namespace cpictures {

bool CopyFileToClipboard(HWND owner, const std::filesystem::path& path);
bool CopyTextToClipboard(HWND owner, const std::wstring& text);

}  // namespace cpictures
