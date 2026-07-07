#include "viewer/viewer_window.h"

#include <algorithm>
#include <cstdlib>
#include <exception>

#include <dwmapi.h>
#include <windowsx.h>

#include "cpictures/overlay.h"
#include "platform/clipboard_service.h"
#include "platform/context_menu.h"
#include "update/update_service.h"

namespace cpictures {
namespace {

constexpr wchar_t kWindowClassName[] = L"cpictures.viewer";
constexpr UINT_PTR kOverlayClickTimer = 1;
constexpr int kFullscreenOverscan = 8;

void ApplyWindowFrameStyle(HWND hwnd) {
    const DWMNCRENDERINGPOLICY renderingPolicy = DWMNCRP_ENABLED;
    DwmSetWindowAttribute(
        hwnd,
        DWMWA_NCRENDERING_POLICY,
        &renderingPolicy,
        sizeof(renderingPolicy));

    const COLORREF borderColor = DWMWA_COLOR_DEFAULT;
    DwmSetWindowAttribute(hwnd, DWMWA_BORDER_COLOR, &borderColor, sizeof(borderColor));

    const DWM_WINDOW_CORNER_PREFERENCE cornerPreference = DWMWCP_DEFAULT;
    DwmSetWindowAttribute(
        hwnd,
        DWMWA_WINDOW_CORNER_PREFERENCE,
        &cornerPreference,
        sizeof(cornerPreference));

    MARGINS margins{1, 1, 1, 1};
    DwmExtendFrameIntoClientArea(hwnd, &margins);
}

void ApplyFullscreenFrameStyle(HWND hwnd) {
    const DWMNCRENDERINGPOLICY renderingPolicy = DWMNCRP_DISABLED;
    DwmSetWindowAttribute(
        hwnd,
        DWMWA_NCRENDERING_POLICY,
        &renderingPolicy,
        sizeof(renderingPolicy));

    const COLORREF borderColor = DWMWA_COLOR_NONE;
    DwmSetWindowAttribute(hwnd, DWMWA_BORDER_COLOR, &borderColor, sizeof(borderColor));

    const DWM_WINDOW_CORNER_PREFERENCE cornerPreference = DWMWCP_DONOTROUND;
    DwmSetWindowAttribute(
        hwnd,
        DWMWA_WINDOW_CORNER_PREFERENCE,
        &cornerPreference,
        sizeof(cornerPreference));

    MARGINS margins{0, 0, 0, 0};
    DwmExtendFrameIntoClientArea(hwnd, &margins);
}

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
    wc.style = CS_DBLCLKS;
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
        WS_OVERLAPPEDWINDOW,
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
    ApplyWindowFrameStyle(hwnd_);

    try {
        LoadCurrentImage(true);
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
    case WM_NCCALCSIZE:
        if (wparam == TRUE && !viewState_.fullscreen) {
            return 0;
        }
        break;
    case WM_NCHITTEST: {
        const LRESULT hit = DefWindowProcW(hwnd_, message, wparam, lparam);
        return hit;
    }
    case WM_KEYDOWN:
        if (wparam == VK_ESCAPE) {
            Close();
            return 0;
        }
        if (wparam == VK_UP) {
            if (CanPanCurrentImage()) {
                const SizeI draw = CurrentDrawSize();
                const SizeI client = CurrentClientSize();
                if (draw.height > client.height) {
                    PanBy(0, -96);
                } else {
                    PanBy(-96, 0);
                }
                return 0;
            }
        }
        if (wparam == VK_DOWN) {
            if (CanPanCurrentImage()) {
                const SizeI draw = CurrentDrawSize();
                const SizeI client = CurrentClientSize();
                if (draw.height > client.height) {
                    PanBy(0, 96);
                } else {
                    PanBy(96, 0);
                }
                return 0;
            }
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
    case WM_LBUTTONDOWN:
        if ((GetKeyState(VK_SHIFT) & 0x8000) != 0 && CanPanCurrentImage()) {
            panningImage_ = true;
            panDragStartPoint_ = {GET_X_LPARAM(lparam), GET_Y_LPARAM(lparam)};
            panDragStartOffset_ = {viewState_.panX, viewState_.panY};
            SetCapture(hwnd_);
            SetCursor(LoadCursorW(nullptr, IDC_HAND));
            return 0;
        }
        leftButtonDown_ = true;
        leftButtonDownPoint_ = {GET_X_LPARAM(lparam), GET_Y_LPARAM(lparam)};
        SetCapture(hwnd_);
        return 0;
    case WM_LBUTTONDBLCLK:
        leftButtonDown_ = false;
        panningImage_ = false;
        ReleaseCapture();
        KillTimer(hwnd_, kOverlayClickTimer);
        return 0;
    case WM_MOUSEMOVE:
        if (panningImage_ && (wparam & MK_LBUTTON) != 0) {
            const int dx = GET_X_LPARAM(lparam) - panDragStartPoint_.x;
            const int dy = GET_Y_LPARAM(lparam) - panDragStartPoint_.y;
            PanTo({panDragStartOffset_.x - dx, panDragStartOffset_.y - dy});
            SetCursor(LoadCursorW(nullptr, IDC_HAND));
            return 0;
        }
        if (leftButtonDown_ && (wparam & MK_LBUTTON) != 0) {
            const int dx = GET_X_LPARAM(lparam) - leftButtonDownPoint_.x;
            const int dy = GET_Y_LPARAM(lparam) - leftButtonDownPoint_.y;
            const int thresholdX = GetSystemMetrics(SM_CXDRAG);
            const int thresholdY = GetSystemMetrics(SM_CYDRAG);
            if (std::abs(dx) >= thresholdX || std::abs(dy) >= thresholdY) {
                leftButtonDown_ = false;
                ReleaseCapture();
                KillTimer(hwnd_, kOverlayClickTimer);
                POINT point{};
                GetCursorPos(&point);
                SendMessageW(hwnd_, WM_NCLBUTTONDOWN, HTCAPTION, MAKELPARAM(point.x, point.y));
            }
        }
        return 0;
    case WM_LBUTTONUP:
        if (panningImage_) {
            panningImage_ = false;
            ReleaseCapture();
            return 0;
        }
        if (leftButtonDown_) {
            leftButtonDown_ = false;
            ReleaseCapture();
            SetTimer(hwnd_, kOverlayClickTimer, GetDoubleClickTime(), nullptr);
        }
        return 0;
    case WM_SETCURSOR:
        if ((GetKeyState(VK_SHIFT) & 0x8000) != 0 && CanPanCurrentImage()) {
            SetCursor(LoadCursorW(nullptr, IDC_HAND));
            return TRUE;
        }
        break;
    case WM_TIMER:
        if (wparam == kOverlayClickTimer) {
            KillTimer(hwnd_, kOverlayClickTimer);
            ToggleOverlay();
            return 0;
        }
        break;
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
        if (const auto command = ShowContextMenu(hwnd_, point, viewState_.fullscreen)) {
            ExecuteCommand(*command);
        }
        return 0;
    }
    case WM_SIZE:
        renderer_.Resize(hwnd_);
        ClampCurrentPan();
        return 0;
    case WM_DPICHANGED: {
        auto* suggested = reinterpret_cast<RECT*>(lparam);
        SetWindowPos(
            hwnd_,
            nullptr,
            suggested->left,
            suggested->top,
            suggested->right - suggested->left,
            suggested->bottom - suggested->top,
            SWP_NOZORDER | SWP_NOACTIVATE);
        renderer_.Resize(hwnd_);
        ClampCurrentPan();
        return 0;
    }
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

void ViewerWindow::LoadCurrentImage(bool centerOnMonitor) {
    if (auto prefetched = prefetcher_.Take(imageList_.Current())) {
        decoded_ = std::move(*prefetched);
    } else {
        decoded_ = decoder_.Decode(imageList_.Current());
    }

    ResetPan();
    if (centerOnMonitor) {
        ResizeWindowToClientSize(ActualSizeViewport(decoded_.size, WorkAreaForWindow()), true);
    } else if (viewState_.fitMode == FitMode::FitToScreen) {
        const SizeI fit = ScaleImageToFitWorkArea(decoded_.size, WorkAreaForWindow());
        if (IsValid(fit)) {
            viewState_.zoom = static_cast<double>(fit.width) / static_cast<double>(decoded_.size.width);
            ResizeWindowToClientSize(fit, false);
        }
    } else {
        ResizeWindowForCurrentZoom();
    }

    renderer_.EnsureDevice(hwnd_);
    renderer_.SetImage(decoded_);
    ClampCurrentPan();
    WarmPrefetch();
}

void ViewerWindow::ResizeWindowToClientSize(SizeI targetSize, bool centerOnMonitor) {
    if (!IsValid(targetSize)) {
        return;
    }

    RECT oldRect{};
    GetWindowRect(hwnd_, &oldRect);

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
    const int x = centerOnMonitor
        ? info.rcWork.left + ((info.rcWork.right - info.rcWork.left) - finalWidth) / 2
        : oldRect.left + ((oldRect.right - oldRect.left) - finalWidth) / 2;
    const int y = centerOnMonitor
        ? info.rcWork.top + ((info.rcWork.bottom - info.rcWork.top) - finalHeight) / 2
        : oldRect.top + ((oldRect.bottom - oldRect.top) - finalHeight) / 2;
    SetWindowPos(hwnd_, nullptr, x, y, finalWidth, finalHeight, SWP_NOZORDER | SWP_NOACTIVATE);
    renderer_.Resize(hwnd_);
    ClampCurrentPan();
}

void ViewerWindow::ResizeWindowForCurrentZoom() {
    if (!IsValid(decoded_.size) || viewState_.fullscreen) {
        return;
    }
    const SizeI target = viewState_.zoom == 1.0
        ? ActualSizeViewport(decoded_.size, WorkAreaForWindow())
        : ScaleImageByZoomToWorkArea(decoded_.size, WorkAreaForWindow(), viewState_.zoom);
    ResizeWindowToClientSize(target, false);
}

void ViewerWindow::ResetPan() {
    viewState_.panX = 0;
    viewState_.panY = 0;
}

void ViewerWindow::ClampCurrentPan() {
    const PointI clamped = ClampPanOffset(
        {viewState_.panX, viewState_.panY},
        CurrentDrawSize(),
        CurrentClientSize());
    viewState_.panX = clamped.x;
    viewState_.panY = clamped.y;
}

void ViewerWindow::PanTo(PointI offset) {
    const PointI clamped = ClampPanOffset(offset, CurrentDrawSize(), CurrentClientSize());
    if (viewState_.panX == clamped.x && viewState_.panY == clamped.y) {
        return;
    }
    viewState_.panX = clamped.x;
    viewState_.panY = clamped.y;
    InvalidateRect(hwnd_, nullptr, FALSE);
}

void ViewerWindow::PanBy(int dx, int dy) {
    PanTo({viewState_.panX + dx, viewState_.panY + dy});
}

bool ViewerWindow::CanPanCurrentImage() const {
    return CanPan(CurrentDrawSize(), CurrentClientSize());
}

SizeI ViewerWindow::CurrentDrawSize() const {
    if (!IsValid(decoded_.size) || viewState_.zoom <= 0.0) {
        return {};
    }
    return {
        std::max(1, static_cast<int>(decoded_.size.width * viewState_.zoom + 0.5)),
        std::max(1, static_cast<int>(decoded_.size.height * viewState_.zoom + 0.5))
    };
}

SizeI ViewerWindow::CurrentClientSize() const {
    if (hwnd_ == nullptr) {
        return {};
    }
    RECT client{};
    GetClientRect(hwnd_, &client);
    return {client.right - client.left, client.bottom - client.top};
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
    viewState_.rotationDegrees = 0;
    LoadCurrentImage(false);
}

void ViewerWindow::ShowPreviousImage() {
    imageList_.Previous();
    viewState_.rotationDegrees = 0;
    LoadCurrentImage(false);
}

void ViewerWindow::ApplyZoom(double factor) {
    viewState_.fitMode = FitMode::ActualSize;
    viewState_.zoom = std::clamp(viewState_.zoom * factor, 0.05, 32.0);
    ResizeWindowForCurrentZoom();
}

void ViewerWindow::SetActualSize() {
    viewState_.fitMode = FitMode::ActualSize;
    viewState_.zoom = 1.0;
    ResetPan();
    ResizeWindowForCurrentZoom();
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
    ResetPan();
    ResizeWindowToClientSize(fit, false);
}

void ViewerWindow::ToggleFullscreen() {
    if (!viewState_.fullscreen) {
        GetWindowRect(hwnd_, &restoreRect_);
        restoreStyle_ = GetWindowLongPtrW(hwnd_, GWL_STYLE);
        restoreExStyle_ = GetWindowLongPtrW(hwnd_, GWL_EXSTYLE);
        hasRestoreRect_ = true;

        HMONITOR monitor = MonitorFromWindow(hwnd_, MONITOR_DEFAULTTONEAREST);
        MONITORINFO info{sizeof(info)};
        GetMonitorInfoW(monitor, &info);
        viewState_.fullscreen = true;
        SetWindowLongPtrW(hwnd_, GWL_STYLE, WS_POPUP | WS_VISIBLE);
        SetWindowLongPtrW(hwnd_, GWL_EXSTYLE, WS_EX_APPWINDOW);
        SetWindowPos(
            hwnd_,
            HWND_TOPMOST,
            info.rcMonitor.left - kFullscreenOverscan,
            info.rcMonitor.top - kFullscreenOverscan,
            info.rcMonitor.right - info.rcMonitor.left + kFullscreenOverscan * 2,
            info.rcMonitor.bottom - info.rcMonitor.top + kFullscreenOverscan * 2,
            SWP_FRAMECHANGED);
    ApplyFullscreenFrameStyle(hwnd_);
    InvalidateRect(hwnd_, nullptr, FALSE);
    renderer_.Resize(hwnd_);
    ClampCurrentPan();
    return;
    }

    if (!hasRestoreRect_) {
        return;
    }

    SetWindowLongPtrW(hwnd_, GWL_STYLE, restoreStyle_);
    SetWindowLongPtrW(hwnd_, GWL_EXSTYLE, restoreExStyle_);
    viewState_.fullscreen = false;
    SetWindowPos(
        hwnd_,
        HWND_NOTOPMOST,
        restoreRect_.left,
        restoreRect_.top,
        restoreRect_.right - restoreRect_.left,
        restoreRect_.bottom - restoreRect_.top,
        SWP_NOZORDER | SWP_FRAMECHANGED);
    ApplyWindowFrameStyle(hwnd_);
    renderer_.Resize(hwnd_);
    ClampCurrentPan();
}

void ViewerWindow::RotateBy(int degrees) {
    viewState_.rotationDegrees = (viewState_.rotationDegrees + degrees + 360) % 360;
}

void ViewerWindow::ToggleOverlay() {
    viewState_.overlayVisible = !viewState_.overlayVisible;
    InvalidateRect(hwnd_, nullptr, FALSE);
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
