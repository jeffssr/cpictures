#include "app/app.h"

#include <shellapi.h>

#include <filesystem>
#include <string_view>

#include "viewer/viewer_window.h"

namespace cpictures {
namespace {

std::filesystem::path ExtractImagePath(std::wstring_view commandLine) {
    int argc = 0;
    PWSTR* argv = CommandLineToArgvW(commandLine.data(), &argc);
    std::filesystem::path path;
    if (argv != nullptr) {
        if (argc > 1 && argv[1] != nullptr) {
            path = argv[1];
        }
        LocalFree(argv);
    }
    return path;
}

}  // namespace

int RunApplication(HINSTANCE instance, int showCommand, std::wstring_view commandLine) {
    const std::filesystem::path path = ExtractImagePath(commandLine);
    if (path.empty()) {
        MessageBoxW(nullptr, L"cpictures: pass an image path.", L"cpictures", MB_OK | MB_ICONINFORMATION);
        return 1;
    }

    ViewerWindow window(instance, showCommand);
    return window.CreateAndShow(path);
}

}  // namespace cpictures
