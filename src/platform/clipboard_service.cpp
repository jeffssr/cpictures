#include "platform/clipboard_service.h"

#include <cstring>

#include <shlobj_core.h>

namespace cpictures {
namespace {

class ClipboardScope {
public:
    explicit ClipboardScope(HWND owner) {
        for (int attempt = 0; attempt < 20; ++attempt) {
            if (OpenClipboard(owner) != FALSE) {
                opened_ = true;
                return;
            }
            Sleep(10);
        }
    }

    ClipboardScope(const ClipboardScope&) = delete;
    ClipboardScope& operator=(const ClipboardScope&) = delete;

    ~ClipboardScope() {
        if (opened_) {
            CloseClipboard();
        }
    }

    [[nodiscard]] bool IsOpen() const { return opened_; }

private:
    bool opened_ = false;
};

}  // namespace

bool CopyFileToClipboard(HWND owner, const std::filesystem::path& path) {
    const std::wstring fullPath = std::filesystem::absolute(path).wstring();
    const SIZE_T bytes = sizeof(DROPFILES) + (fullPath.size() + 2) * sizeof(wchar_t);
    HGLOBAL memory = GlobalAlloc(GMEM_MOVEABLE | GMEM_ZEROINIT, bytes);
    if (memory == nullptr) {
        return false;
    }

    auto* dropFiles = static_cast<DROPFILES*>(GlobalLock(memory));
    if (dropFiles == nullptr) {
        GlobalFree(memory);
        return false;
    }

    dropFiles->pFiles = sizeof(DROPFILES);
    dropFiles->fWide = TRUE;
    auto* fileList = reinterpret_cast<wchar_t*>(reinterpret_cast<BYTE*>(dropFiles) + sizeof(DROPFILES));
    std::memcpy(fileList, fullPath.c_str(), fullPath.size() * sizeof(wchar_t));
    GlobalUnlock(memory);

    ClipboardScope clipboard(owner);
    if (!clipboard.IsOpen()) {
        GlobalFree(memory);
        return false;
    }

    if (!EmptyClipboard()) {
        GlobalFree(memory);
        return false;
    }

    if (SetClipboardData(CF_HDROP, memory) == nullptr) {
        GlobalFree(memory);
        return false;
    }

    return true;
}

bool CopyTextToClipboard(HWND owner, const std::wstring& text) {
    const SIZE_T bytes = (text.size() + 1) * sizeof(wchar_t);
    HGLOBAL memory = GlobalAlloc(GMEM_MOVEABLE | GMEM_ZEROINIT, bytes);
    if (memory == nullptr) {
        return false;
    }

    auto* buffer = static_cast<wchar_t*>(GlobalLock(memory));
    if (buffer == nullptr) {
        GlobalFree(memory);
        return false;
    }

    std::memcpy(buffer, text.c_str(), text.size() * sizeof(wchar_t));
    GlobalUnlock(memory);

    ClipboardScope clipboard(owner);
    if (!clipboard.IsOpen()) {
        GlobalFree(memory);
        return false;
    }

    if (!EmptyClipboard()) {
        GlobalFree(memory);
        return false;
    }

    if (SetClipboardData(CF_UNICODETEXT, memory) == nullptr) {
        GlobalFree(memory);
        return false;
    }

    return true;
}

}  // namespace cpictures
