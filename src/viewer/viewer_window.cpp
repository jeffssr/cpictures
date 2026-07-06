#include "viewer/viewer_window.h"

#include <exception>

#include "cpictures/overlay.h"

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

    try {
        LoadCurrentImage();
    } catch (const std::exception&) {
        MessageBoxW(hwnd_, L"cpictures: failed to open image.", L"cpictures", MB_OK | MB_ICONERROR);
        DestroyWindow(hwnd_);
        hwnd_ = nullptr;
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
    case WM_LBUTTONUP:
        viewState_.overlayVisible = !viewState_.overlayVisible;
        InvalidateRect(hwnd_, nullptr, FALSE);
        return 0;
    case WM_SIZE:
        renderer_.Resize(hwnd_);
        return 0;
    case WM_PAINT: {
        PAINTSTRUCT ps{};
        BeginPaint(hwnd_, &ps);
        renderer_.Render(hwnd_, viewState_, OverlayText());
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

void ViewerWindow::LoadCurrentImage() {
    decoded_ = decoder_.Decode(imageList_.Current());

    const SizeI targetSize = FitImageWindow(decoded_.size, WorkAreaForWindow());
    SetWindowPos(hwnd_, nullptr, 0, 0, targetSize.width, targetSize.height, SWP_NOMOVE | SWP_NOZORDER | SWP_NOACTIVATE);

    RECT client{};
    GetClientRect(hwnd_, &client);
    const int clientWidth = client.right - client.left;
    const int clientHeight = client.bottom - client.top;
    if (clientWidth != targetSize.width || clientHeight != targetSize.height) {
        const int correctedWidth = targetSize.width + (targetSize.width - clientWidth);
        const int correctedHeight = targetSize.height + (targetSize.height - clientHeight);
        SetWindowPos(hwnd_, nullptr, 0, 0, correctedWidth, correctedHeight, SWP_NOMOVE | SWP_NOZORDER | SWP_NOACTIVATE);
        GetClientRect(hwnd_, &client);
    }

    HMONITOR monitor = MonitorFromWindow(hwnd_, MONITOR_DEFAULTTONEAREST);
    MONITORINFO info{sizeof(info)};
    GetMonitorInfoW(monitor, &info);

    const int finalWidth = client.right - client.left;
    const int finalHeight = client.bottom - client.top;
    const int x = info.rcWork.left + ((info.rcWork.right - info.rcWork.left) - finalWidth) / 2;
    const int y = info.rcWork.top + ((info.rcWork.bottom - info.rcWork.top) - finalHeight) / 2;
    SetWindowPos(hwnd_, nullptr, x, y, finalWidth, finalHeight, SWP_NOZORDER | SWP_NOACTIVATE);

    renderer_.EnsureDevice(hwnd_);
    renderer_.SetImage(decoded_);
}

SizeI ViewerWindow::WorkAreaForWindow() const {
    HMONITOR monitor = MonitorFromWindow(hwnd_, MONITOR_DEFAULTTONEAREST);
    MONITORINFO info{sizeof(info)};
    GetMonitorInfoW(monitor, &info);
    return {info.rcWork.right - info.rcWork.left, info.rcWork.bottom - info.rcWork.top};
}

std::wstring ViewerWindow::OverlayText() const {
    return BuildOverlayText(
        imageList_.Current().filename().wstring(),
        static_cast<int>(imageList_.Index()),
        static_cast<int>(imageList_.Count()));
}

}  // namespace cpictures
