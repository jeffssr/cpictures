#pragma once

#include <filesystem>
#include <string>
#include <windows.h>

namespace cpictures {

std::wstring ComputeSha256(const std::filesystem::path& path);
void ShowFormatSupportDialog(HWND owner);

}  // namespace cpictures
