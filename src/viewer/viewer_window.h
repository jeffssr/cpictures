#pragma once

#include <filesystem>

#include <windows.h>

#include "cpictures/image_list.h"
#include "cpictures/view_state.h"
#include "image/wic_decoder.h"

namespace cpictures {

class ViewerWindow {
public:
    ViewerWindow(HINSTANCE instance, int showCommand);
    int CreateAndShow(const std::filesystem::path& path);

private:
    static LRESULT CALLBACK WindowProc(HWND hwnd, UINT message, WPARAM wparam, LPARAM lparam);
    LRESULT HandleMessage(UINT message, WPARAM wparam, LPARAM lparam);
    void Close();

    HINSTANCE instance_ = nullptr;
    int showCommand_ = SW_SHOWNORMAL;
    HWND hwnd_ = nullptr;
    ImageList imageList_;
    ViewState viewState_;
    WicDecoder decoder_;
};

}  // namespace cpictures
