#include "app/app.h"

#include <shellapi.h>
#include <windows.h>

#include <exception>
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
    try {
        const std::filesystem::path path = ExtractImagePath(commandLine);
        if (path.empty()) {
            MessageBoxW(nullptr, L"cpictures: pass an image path.", L"cpictures", MB_OK | MB_ICONINFORMATION);
            return 1;
        }

        ViewerWindow window(instance, showCommand);
        return window.CreateAndShow(path);
    } catch (const std::exception&) {
        MessageBoxW(nullptr, L"cpictures: startup failed.", L"cpictures", MB_OK | MB_ICONERROR);
        return 1;
    } catch (...) {
        MessageBoxW(nullptr, L"cpictures: startup failed.", L"cpictures", MB_OK | MB_ICONERROR);
        return 1;
    }
}

}  // namespace cpictures
