#pragma once

#include <optional>

#include <windows.h>

#include "cpictures/commands.h"

namespace cpictures {

std::optional<Command> ShowContextMenu(HWND owner, POINT screenPoint);

}  // namespace cpictures
