#pragma once

#include <filesystem>
#include <string>

#include <windows.h>

#include "cpictures/image_list.h"
#include "cpictures/geometry.h"
#include "cpictures/view_state.h"
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
    void LoadCurrentImage();
    SizeI WorkAreaForWindow() const;
    std::wstring OverlayText() const;

    HINSTANCE instance_ = nullptr;
    int showCommand_ = SW_SHOWNORMAL;
    HWND hwnd_ = nullptr;
    ImageList imageList_;
    ViewState viewState_;
    WicDecoder decoder_;
    DecodedImage decoded_;
    D2DRenderer renderer_;
};

}  // namespace cpictures
