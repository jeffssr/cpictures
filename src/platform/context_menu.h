#pragma once

#include <optional>

#include <windows.h>

#include "cpictures/commands.h"

namespace cpictures {

[[nodiscard]] constexpr const wchar_t* FullscreenMenuText(bool fullscreen) noexcept {
    return fullscreen ? L"\x53D6\x6D88\x5168\x5C4F" : L"\x5168\x5C4F";
}

std::optional<Command> ShowContextMenu(HWND owner, POINT screenPoint, bool fullscreen);

}  // namespace cpictures
