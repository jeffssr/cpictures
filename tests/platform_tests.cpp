#include <cstdlib>
#include <cstring>
#include <filesystem>
#include <iostream>
#include <string>
#include <utility>
#include <vector>

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

HGLOBAL DuplicateClipboardHandle(HANDLE source) {
    if (source == nullptr) {
        return nullptr;
    }

    const SIZE_T size = GlobalSize(source);
    if (size == 0) {
        return nullptr;
    }

    const void* sourceBytes = GlobalLock(source);
    if (sourceBytes == nullptr) {
        return nullptr;
    }

    HGLOBAL copy = GlobalAlloc(GMEM_MOVEABLE, size);
    if (copy == nullptr) {
        GlobalUnlock(source);
        return nullptr;
    }

    void* destinationBytes = GlobalLock(copy);
    if (destinationBytes == nullptr) {
        GlobalUnlock(source);
        GlobalFree(copy);
        return nullptr;
    }

    std::memcpy(destinationBytes, sourceBytes, size);
    GlobalUnlock(copy);
    GlobalUnlock(source);
    return copy;
}

class ClipboardSnapshot {
public:
    explicit ClipboardSnapshot(HWND owner) {
        if (!OpenClipboardWithRetry(owner)) {
            skipReason_ = "platform tests skipped: clipboard access denied in this environment";
            return;
        }

        for (UINT format = 0; (format = EnumClipboardFormats(format)) != 0;) {
            HANDLE handle = GetClipboardData(format);
            if (handle == nullptr) {
                skipReason_ = "platform tests skipped: failed to read clipboard format";
                CloseClipboard();
                ClearOwnedHandles();
                return;
            }

            HGLOBAL copy = DuplicateClipboardHandle(handle);
            if (copy == nullptr) {
                skipReason_ = "platform tests skipped: clipboard contains unsupported non-HGLOBAL data";
                CloseClipboard();
                ClearOwnedHandles();
                return;
            }

            formats_.push_back({format, copy});
        }

        CloseClipboard();
        restorable_ = true;
    }

    ClipboardSnapshot(const ClipboardSnapshot&) = delete;
    ClipboardSnapshot& operator=(const ClipboardSnapshot&) = delete;

    ~ClipboardSnapshot() {
        if (!restorable_) {
            ClearOwnedHandles();
            return;
        }

        if (!OpenClipboardWithRetry(nullptr)) {
            ClearOwnedHandles();
            return;
        }

        EmptyClipboard();
        for (auto& entry : formats_) {
            if (SetClipboardData(entry.first, entry.second) != nullptr) {
                entry.second = nullptr;
            }
        }
        CloseClipboard();
        ClearOwnedHandles();
    }

    [[nodiscard]] bool ShouldSkip() const { return !skipReason_.empty(); }
    [[nodiscard]] const char* SkipReason() const { return skipReason_.c_str(); }

private:
    void ClearOwnedHandles() {
        for (auto& entry : formats_) {
            if (entry.second != nullptr) {
                GlobalFree(entry.second);
                entry.second = nullptr;
            }
        }
    }

    bool restorable_ = false;
    std::string skipReason_;
    std::vector<std::pair<UINT, HGLOBAL>> formats_;
};

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
}  // namespace

int main() {
    ClipboardOwnerWindow owner;
    Expect(owner.IsValid(), "clipboard owner window created");

    ClipboardSnapshot snapshot(owner.handle());
    if (snapshot.ShouldSkip()) {
        std::cout << snapshot.SkipReason() << "\n";
        return 0;
    }

    TestCopyTextToClipboard(owner.handle());
    TestCopyFileToClipboard(owner.handle());
    std::cout << "platform tests passed\n";
    return 0;
}
