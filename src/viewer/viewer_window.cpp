#include "viewer/viewer_window.h"

#include <algorithm>
#include <exception>

#include <windowsx.h>

#include "cpictures/overlay.h"
#include "platform/clipboard_service.h"
#include "platform/context_menu.h"
#include "update/update_service.h"

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
        if (wparam == VK_LEFT || wparam == VK_PRIOR) {
            ExecuteCommand(Command::PreviousImage);
            return 0;
        }
        if (wparam == VK_RIGHT || wparam == VK_NEXT) {
            ExecuteCommand(Command::NextImage);
            return 0;
        }
        if (wparam == L'1') {
            ExecuteCommand(Command::ActualSize);
            return 0;
        }
        if (wparam == L'0') {
            ExecuteCommand(Command::FitToScreen);
            return 0;
        }
        if (wparam == VK_F11) {
            ExecuteCommand(Command::ToggleFullscreen);
            return 0;
        }
        if (wparam == VK_OEM_PLUS || wparam == VK_ADD) {
            ExecuteCommand(Command::ZoomIn);
            return 0;
        }
        if (wparam == VK_OEM_MINUS || wparam == VK_SUBTRACT) {
            ExecuteCommand(Command::ZoomOut);
            return 0;
        }
        if ((GetKeyState(VK_CONTROL) & 0x8000) != 0) {
            if (wparam == L'C') {
                if ((GetKeyState(VK_SHIFT) & 0x8000) != 0) {
                    ExecuteCommand(Command::CopyPath);
                } else {
                    ExecuteCommand(Command::CopyFile);
                }
                return 0;
            }
            if (wparam == L'L') {
                ExecuteCommand(Command::RotateLeft);
                return 0;
            }
            if (wparam == L'R') {
                ExecuteCommand(Command::RotateRight);
                return 0;
            }
        }
        break;
    case WM_ERASEBKGND:
        return 1;
    case WM_LBUTTONUP:
        viewState_.overlayVisible = !viewState_.overlayVisible;
        InvalidateRect(hwnd_, nullptr, FALSE);
        return 0;
    case WM_MOUSEWHEEL: {
        const short delta = GET_WHEEL_DELTA_WPARAM(wparam);
        if ((GET_KEYSTATE_WPARAM(wparam) & MK_CONTROL) != 0) {
            ExecuteCommand(delta > 0 ? Command::ZoomIn : Command::ZoomOut);
        } else {
            ExecuteCommand(delta > 0 ? Command::PreviousImage : Command::NextImage);
        }
        return 0;
    }
    case WM_CONTEXTMENU: {
        POINT point{GET_X_LPARAM(lparam), GET_Y_LPARAM(lparam)};
        if (point.x == -1 && point.y == -1) {
            RECT rect{};
            GetWindowRect(hwnd_, &rect);
            point = {rect.left + 24, rect.top + 24};
        }
        if (const auto command = ShowContextMenu(hwnd_, point)) {
            ExecuteCommand(*command);
        }
        return 0;
    }
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

void ViewerWindow::ExecuteCommand(Command command) {
    switch (command) {
    case Command::PreviousImage:
        ShowPreviousImage();
        break;
    case Command::NextImage:
        ShowNextImage();
        break;
    case Command::ZoomIn:
        ApplyZoom(1.1);
        break;
    case Command::ZoomOut:
        ApplyZoom(1.0 / 1.1);
        break;
    case Command::ActualSize:
        SetActualSize();
        break;
    case Command::FitToScreen:
        SetFitToScreen();
        break;
    case Command::ToggleFullscreen:
        ToggleFullscreen();
        break;
    case Command::RotateLeft:
        RotateBy(-90);
        break;
    case Command::RotateRight:
        RotateBy(90);
        break;
    case Command::CopyFile:
        CopyFileToClipboard(hwnd_, imageList_.Current());
        break;
    case Command::CopyPath:
        CopyTextToClipboard(hwnd_, std::filesystem::absolute(imageList_.Current()).wstring());
        break;
    case Command::InstallOrUpdateFormats:
        ShowFormatSupportDialog(hwnd_);
        break;
    }

    InvalidateRect(hwnd_, nullptr, FALSE);
}

void ViewerWindow::LoadCurrentImage() {
    if (auto prefetched = prefetcher_.Take(imageList_.Current())) {
        decoded_ = std::move(*prefetched);
    } else {
        decoded_ = decoder_.Decode(imageList_.Current());
    }

    ResizeWindowToClientSize(FitImageWindow(decoded_.size, WorkAreaForWindow()));
    renderer_.EnsureDevice(hwnd_);
    renderer_.SetImage(decoded_);
    WarmPrefetch();
}

void ViewerWindow::ResizeWindowToClientSize(SizeI targetSize) {
    if (!IsValid(targetSize)) {
        return;
    }

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
}

void ViewerWindow::WarmPrefetch() {
    if (imageList_.Count() < 2) {
        prefetcher_.Cancel();
        return;
    }

    ImageList copy = imageList_;
    const auto next = copy.Next();
    copy = imageList_;
    const auto previous = copy.Previous();
    prefetcher_.Warm(previous, next);
}

void ViewerWindow::ShowNextImage() {
    imageList_.Next();
    viewState_.zoom = 1.0;
    viewState_.rotationDegrees = 0;
    viewState_.fitMode = FitMode::ActualSize;
    LoadCurrentImage();
}

void ViewerWindow::ShowPreviousImage() {
    imageList_.Previous();
    viewState_.zoom = 1.0;
    viewState_.rotationDegrees = 0;
    viewState_.fitMode = FitMode::ActualSize;
    LoadCurrentImage();
}

void ViewerWindow::ApplyZoom(double factor) {
    viewState_.fitMode = FitMode::ActualSize;
    viewState_.zoom = std::clamp(viewState_.zoom * factor, 0.05, 32.0);
}

void ViewerWindow::SetActualSize() {
    viewState_.fitMode = FitMode::ActualSize;
    viewState_.zoom = 1.0;
}

void ViewerWindow::SetFitToScreen() {
    if (!IsValid(decoded_.size)) {
        viewState_.fitMode = FitMode::ActualSize;
        viewState_.zoom = 1.0;
        return;
    }

    const SizeI work = WorkAreaForWindow();
    const SizeI fit = ScaleImageToFitWorkArea(decoded_.size, work);
    if (!IsValid(fit)) {
        viewState_.fitMode = FitMode::ActualSize;
        viewState_.zoom = 1.0;
        return;
    }

    viewState_.fitMode = FitMode::FitToScreen;
    viewState_.zoom = static_cast<double>(fit.width) / static_cast<double>(decoded_.size.width);
    ResizeWindowToClientSize(fit);
}

void ViewerWindow::ToggleFullscreen() {
    if (!viewState_.fullscreen) {
        GetWindowRect(hwnd_, &restoreRect_);
        hasRestoreRect_ = true;

        HMONITOR monitor = MonitorFromWindow(hwnd_, MONITOR_DEFAULTTONEAREST);
        MONITORINFO info{sizeof(info)};
        GetMonitorInfoW(monitor, &info);
        SetWindowPos(
            hwnd_,
            HWND_TOP,
            info.rcMonitor.left,
            info.rcMonitor.top,
            info.rcMonitor.right - info.rcMonitor.left,
            info.rcMonitor.bottom - info.rcMonitor.top,
            SWP_FRAMECHANGED);
        viewState_.fullscreen = true;
        return;
    }

    if (!hasRestoreRect_) {
        return;
    }

    SetWindowPos(
        hwnd_,
        nullptr,
        restoreRect_.left,
        restoreRect_.top,
        restoreRect_.right - restoreRect_.left,
        restoreRect_.bottom - restoreRect_.top,
        SWP_NOZORDER | SWP_FRAMECHANGED);
    viewState_.fullscreen = false;
}

void ViewerWindow::RotateBy(int degrees) {
    viewState_.rotationDegrees = (viewState_.rotationDegrees + degrees + 360) % 360;
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
