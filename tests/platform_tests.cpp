#include <cstdlib>
#include <filesystem>
#include <iostream>
#include <string>

#include <windows.h>
#include <shellapi.h>

#include "platform/clipboard_service.h"

namespace {
void Expect(bool condition, const char* message) {
    if (!condition) {
        std::cerr << "FAIL: " << message << "\n";
        std::exit(1);
    }
}

bool OpenClipboardWithRetry(HWND owner) {
    for (int attempt = 0; attempt < 20; ++attempt) {
        if (OpenClipboard(owner)) {
            return true;
        }
        Sleep(10);
    }
    return false;
}

bool ClipboardUnavailableInEnvironment() {
    if (OpenClipboardWithRetry(nullptr)) {
        CloseClipboard();
        return false;
    }
    return GetLastError() == ERROR_ACCESS_DENIED;
}

class ClipboardOwnerWindow {
public:
    ClipboardOwnerWindow() {
        hwnd_ = CreateWindowExW(0, L"STATIC", L"cpictures-clipboard-test", 0, 0, 0, 0, 0, HWND_MESSAGE, nullptr, GetModuleHandleW(nullptr), nullptr);
    }

    ~ClipboardOwnerWindow() {
        if (hwnd_ != nullptr) {
            DestroyWindow(hwnd_);
        }
    }

    [[nodiscard]] HWND handle() const { return hwnd_; }
    [[nodiscard]] bool IsValid() const { return hwnd_ != nullptr; }

private:
    HWND hwnd_ = nullptr;
};

void TestCopyTextToClipboard(HWND owner) {
    const std::wstring expected = L"C:\\temp\\cpictures sample.jpg";
    Expect(cpictures::CopyTextToClipboard(owner, expected), "copy text call succeeds");
    Expect(OpenClipboardWithRetry(owner), "open clipboard for text read");
    HANDLE handle = GetClipboardData(CF_UNICODETEXT);
    Expect(handle != nullptr, "unicode text handle present");
    const auto* text = static_cast<const wchar_t*>(GlobalLock(handle));
    Expect(text != nullptr, "unicode text lock succeeds");
    Expect(expected == text, "unicode text matches source");
    GlobalUnlock(handle);
    CloseClipboard();
}

void TestCopyFileToClipboard(HWND owner) {
    const std::filesystem::path path = std::filesystem::temp_directory_path() / L"cpictures clipboard sample.jpg";
    const std::wstring expected = std::filesystem::absolute(path).wstring();
    Expect(cpictures::CopyFileToClipboard(owner, path), "copy file call succeeds");
    Expect(OpenClipboardWithRetry(owner), "open clipboard for file read");
    HANDLE handle = GetClipboardData(CF_HDROP);
    Expect(handle != nullptr, "file drop handle present");
    const HDROP drop = static_cast<HDROP>(handle);
    Expect(DragQueryFileW(drop, 0xFFFFFFFF, nullptr, 0) == 1, "one file in clipboard");
    const UINT length = DragQueryFileW(drop, 0, nullptr, 0);
    std::wstring actual(length + 1, L'\0');
    Expect(DragQueryFileW(drop, 0, actual.data(), static_cast<UINT>(actual.size())) == length, "read file path from clipboard");
    actual.resize(length);
    Expect(actual == expected, "clipboard file path matches source");
    CloseClipboard();
}
}

int main() {
    if (ClipboardUnavailableInEnvironment()) {
        std::cout << "platform tests skipped: clipboard access denied in this environment\n";
        return 0;
    }
    ClipboardOwnerWindow owner;
    Expect(owner.IsValid(), "clipboard owner window created");
    TestCopyTextToClipboard(owner.handle());
    TestCopyFileToClipboard(owner.handle());
    std::cout << "platform tests passed\n";
    return 0;
}
