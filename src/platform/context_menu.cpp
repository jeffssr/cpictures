#include "platform/context_menu.h"

namespace cpictures {
namespace {

constexpr UINT kRotateLeft = 1001;
constexpr UINT kRotateRight = 1002;
constexpr UINT kCopyFile = 1003;
constexpr UINT kCopyPath = 1004;
constexpr UINT kActualSize = 1005;
constexpr UINT kFitToScreen = 1006;
constexpr UINT kFullscreen = 1007;
constexpr UINT kInstallFormats = 1008;

}  // namespace

std::optional<Command> ShowContextMenu(HWND owner, POINT screenPoint) {
    HMENU menu = CreatePopupMenu();
    if (menu == nullptr) {
        return std::nullopt;
    }

    AppendMenuW(menu, MF_STRING, kRotateLeft, L"\x5411\x5DE6\x65CB\x8F6C\tCtrl+L");
    AppendMenuW(menu, MF_STRING, kRotateRight, L"\x5411\x53F3\x65CB\x8F6C\tCtrl+R");
    AppendMenuW(menu, MF_SEPARATOR, 0, nullptr);
    AppendMenuW(menu, MF_STRING, kCopyFile, L"\x590D\x5236\tCtrl+C");
    AppendMenuW(menu, MF_STRING, kCopyPath, L"\x590D\x5236\x8DEF\x5F84\tCtrl+Shift+C");
    AppendMenuW(menu, MF_SEPARATOR, 0, nullptr);
    AppendMenuW(menu, MF_STRING, kActualSize, L"\x5B9E\x9645\x5927\x5C0F\t1");
    AppendMenuW(menu, MF_STRING, kFitToScreen, L"\x9002\x5E94\x5C4F\x5E55\t0");
    AppendMenuW(menu, MF_STRING, kFullscreen, L"\x5168\x5C4F\tF11");
    AppendMenuW(menu, MF_SEPARATOR, 0, nullptr);
    AppendMenuW(menu, MF_STRING, kInstallFormats, L"\x5B89\x88C5/\x66F4\x65B0\x683C\x5F0F\x652F\x6301...");

    const UINT selected = TrackPopupMenu(
        menu,
        TPM_RETURNCMD | TPM_RIGHTBUTTON,
        screenPoint.x,
        screenPoint.y,
        0,
        owner,
        nullptr);
    DestroyMenu(menu);

    switch (selected) {
    case kRotateLeft:
        return Command::RotateLeft;
    case kRotateRight:
        return Command::RotateRight;
    case kCopyFile:
        return Command::CopyFile;
    case kCopyPath:
        return Command::CopyPath;
    case kActualSize:
        return Command::ActualSize;
    case kFitToScreen:
        return Command::FitToScreen;
    case kFullscreen:
        return Command::ToggleFullscreen;
    case kInstallFormats:
        return Command::InstallOrUpdateFormats;
    default:
        return std::nullopt;
    }
}

}  // namespace cpictures
