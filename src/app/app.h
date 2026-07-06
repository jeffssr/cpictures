#pragma once

#include <string_view>

#include <windows.h>

namespace cpictures {

int RunApplication(HINSTANCE instance, int showCommand, std::wstring_view commandLine);

}  // namespace cpictures
