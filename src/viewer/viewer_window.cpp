#include "viewer/viewer_window.h"

#include <exception>

namespace cpictures {
namespace {

constexpr wchar_t kWindowClassName[] = L"cpictures.viewer";

}  // namespace

ViewerWindow::ViewerWindow(HINSTANCE instance, int showCommand)
    : instance_(instance), showCommand_(showCommand) {}

int ViewerWindow::CreateAndShow(const std::filesystem::path& path) {
    imageList_ = ImageList::LoadFromFile(path);
    if (imageList_.Empty()) {
        MessageBoxW(nullptr, L"cpictures: no image found.", L"cpictures", MB_OK | MB_ICONINFORMATION);
        return 1;
    }

    const auto currentPath = imageList_.Current();
    try {
        decoder_.Decode(currentPath);
    } catch (const std::exception&) {
        MessageBoxW(nullptr, L"cpictures: failed to open image.", L"cpictures", MB_OK | MB_ICONERROR);
        return 1;
    }

    WNDCLASSEXW wc{};
    wc.cbSize = sizeof(wc);
    wc.hInstance = instance_;
    wc.lpfnWndProc = ViewerWindow::WindowProc;
    wc.lpszClassName = kWindowClassName;
    wc.hCursor = LoadCursorW(nullptr, IDC_ARROW);
    wc.hbrBackground = reinterpret_cast<HBRUSH>(COLOR_WINDOW + 1);

    if (!RegisterClassExW(&wc)) {
        const DWORD error = GetLastError();
        if (error != ERROR_CLASS_ALREADY_EXISTS) {
            MessageBoxW(nullptr, L"cpictures: failed to register window class.", L"cpictures", MB_OK | MB_ICONERROR);
            return 1;
        }
    }

    hwnd_ = CreateWindowExW(
        WS_EX_APPWINDOW,
        kWindowClassName,
        L"cpictures",
        WS_POPUP,
        CW_USEDEFAULT,
        CW_USEDEFAULT,
        960,
        640,
        nullptr,
        nullptr,
        instance_,
        this);

    if (hwnd_ == nullptr) {
        MessageBoxW(nullptr, L"cpictures: failed to create window.", L"cpictures", MB_OK | MB_ICONERROR);
        return 1;
    }

    ShowWindow(hwnd_, showCommand_);
    UpdateWindow(hwnd_);

    MSG msg{};
    while (true) {
        const int result = GetMessageW(&msg, nullptr, 0, 0);
        if (result == -1) {
            return 1;
        }
        if (result == 0) {
            return static_cast<int>(msg.wParam);
        }
        TranslateMessage(&msg);
        DispatchMessageW(&msg);
    }
}

LRESULT CALLBACK ViewerWindow::WindowProc(HWND hwnd, UINT message, WPARAM wparam, LPARAM lparam) {
    ViewerWindow* self = nullptr;
    if (message == WM_NCCREATE) {
        auto* create = reinterpret_cast<CREATESTRUCTW*>(lparam);
        self = static_cast<ViewerWindow*>(create->lpCreateParams);
        SetWindowLongPtrW(hwnd, GWLP_USERDATA, reinterpret_cast<LONG_PTR>(self));
        self->hwnd_ = hwnd;
    } else {
        self = reinterpret_cast<ViewerWindow*>(GetWindowLongPtrW(hwnd, GWLP_USERDATA));
    }

    if (self != nullptr) {
        return self->HandleMessage(message, wparam, lparam);
    }
    return DefWindowProcW(hwnd, message, wparam, lparam);
}

LRESULT ViewerWindow::HandleMessage(UINT message, WPARAM wparam, LPARAM lparam) {
    switch (message) {
    case WM_KEYDOWN:
        if (wparam == VK_ESCAPE) {
            Close();
            return 0;
        }
        break;
    case WM_ERASEBKGND:
        return 1;
    case WM_PAINT: {
        PAINTSTRUCT ps{};
        HDC dc = BeginPaint(hwnd_, &ps);
        RECT client{};
        GetClientRect(hwnd_, &client);
        FillRect(dc, &client, reinterpret_cast<HBRUSH>(COLOR_WINDOW + 1));
        EndPaint(hwnd_, &ps);
        return 0;
    }
    case WM_DESTROY:
        PostQuitMessage(0);
        return 0;
    default:
        break;
    }

    return DefWindowProcW(hwnd_, message, wparam, lparam);
}

void ViewerWindow::Close() {
    if (hwnd_ != nullptr) {
        DestroyWindow(hwnd_);
        hwnd_ = nullptr;
    }
}

}  // namespace cpictures
