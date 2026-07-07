#pragma once

#include <filesystem>
#include <string>

#include <windows.h>

#include "cpictures/commands.h"
#include "cpictures/image_list.h"
#include "cpictures/geometry.h"
#include "cpictures/view_state.h"
#include "image/prefetcher.h"
#include "image/wic_decoder.h"
#include "render/d2d_renderer.h"

namespace cpictures {

class ViewerWindow {
public:
    ViewerWindow(HINSTANCE instance, int showCommand);
    int CreateAndShow(const std::filesystem::path& path);

private:
    static LRESULT CALLBACK WindowProc(HWND hwnd, UINT message, WPARAM wparam, LPARAM lparam);
    LRESULT HandleMessage(UINT message, WPARAM wparam, LPARAM lparam);
    void Close();
    void ExecuteCommand(Command command);
    void LoadCurrentImage(bool centerOnMonitor);
    void ResizeWindowToClientSize(SizeI targetSize, bool centerOnMonitor);
    void ResizeWindowForCurrentZoom();
    void ResetPan();
    void ClampCurrentPan();
    void PanTo(PointI offset);
    void PanBy(int dx, int dy);
    bool CanPanCurrentImage() const;
    SizeI CurrentDrawSize() const;
    SizeI CurrentClientSize() const;
    void WarmPrefetch();
    void ShowNextImage();
    void ShowPreviousImage();
    void ApplyZoom(double factor);
    void SetActualSize();
    void SetFitToScreen();
    void ToggleFullscreen();
    void RotateBy(int degrees);
    void ToggleOverlay();
    SizeI WorkAreaForWindow() const;
    std::wstring OverlayText() const;

    HINSTANCE instance_ = nullptr;
    int showCommand_ = SW_SHOWNORMAL;
    HWND hwnd_ = nullptr;
    ImageList imageList_;
    ViewState viewState_;
    Prefetcher prefetcher_;
    WicDecoder decoder_;
    DecodedImage decoded_;
    D2DRenderer renderer_;
    RECT restoreRect_{};
    LONG_PTR restoreStyle_ = 0;
    LONG_PTR restoreExStyle_ = 0;
    bool hasRestoreRect_ = false;
    bool leftButtonDown_ = false;
    POINT leftButtonDownPoint_{};
    bool panningImage_ = false;
    POINT panDragStartPoint_{};
    PointI panDragStartOffset_{};
};

}  // namespace cpictures
