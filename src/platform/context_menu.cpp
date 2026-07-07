#include "platform/context_menu.h"

namespace cpictures {
namespace {

constexpr UINT kCopyFile = 1003;
constexpr UINT kCopyPath = 1004;
constexpr UINT kActualSize = 1005;
constexpr UINT kFitToScreen = 1006;
constexpr UINT kFullscreen = 1007;
constexpr UINT kInstallFormats = 1008;
constexpr UINT kCheckForUpdates = 1009;

}  // namespace

std::optional<Command> ShowContextMenu(HWND owner, POINT screenPoint, bool fullscreen) {
    HMENU menu = CreatePopupMenu();
    if (menu == nullptr) {
        return std::nullopt;
    }

    AppendMenuW(menu, MF_STRING, kCopyFile, L"\x590D\x5236\tCtrl+C");
    AppendMenuW(menu, MF_STRING, kCopyPath, L"\x590D\x5236\x8DEF\x5F84\tCtrl+Shift+C");
    AppendMenuW(menu, MF_SEPARATOR, 0, nullptr);
    AppendMenuW(menu, MF_STRING, kActualSize, L"\x5B9E\x9645\x5927\x5C0F\t1");
    AppendMenuW(menu, MF_STRING, kFitToScreen, L"\x9002\x5E94\x5C4F\x5E55\t0");
    AppendMenuW(menu, MF_STRING, kFullscreen, FullscreenMenuText(fullscreen));
    AppendMenuW(menu, MF_SEPARATOR, 0, nullptr);
    AppendMenuW(menu, MF_STRING, kInstallFormats, L"\x5B89\x88C5/\x66F4\x65B0\x683C\x5F0F\x652F\x6301...");
    AppendMenuW(menu, MF_STRING, kCheckForUpdates, L"\x68C0\x67E5\x66F4\x65B0...");

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
    case kCheckForUpdates:
        return Command::CheckForUpdates;
    default:
        return std::nullopt;
    }
}

}  // namespace cpictures
