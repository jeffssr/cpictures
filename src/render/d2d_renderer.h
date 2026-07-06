#pragma once

#include <string>

#include <d2d1.h>
#include <dwrite.h>
#include <windows.h>
#include <wrl/client.h>

#include "cpictures/view_state.h"
#include "image/wic_decoder.h"

namespace cpictures {

class D2DRenderer {
public:
    D2DRenderer();

    void EnsureDevice(HWND hwnd);
    void Resize(HWND hwnd);
    void SetImage(const DecodedImage& image);
    void Render(HWND hwnd, const ViewState& state, const std::wstring& overlayText);

private:
    void CreateBitmap();
    void DiscardDeviceResources();

    DecodedImage image_;
    Microsoft::WRL::ComPtr<ID2D1Factory> factory_;
    Microsoft::WRL::ComPtr<IDWriteFactory> writeFactory_;
    Microsoft::WRL::ComPtr<ID2D1HwndRenderTarget> target_;
    Microsoft::WRL::ComPtr<ID2D1Bitmap> bitmap_;
    Microsoft::WRL::ComPtr<ID2D1SolidColorBrush> textBrush_;
    Microsoft::WRL::ComPtr<ID2D1SolidColorBrush> badgeBrush_;
    Microsoft::WRL::ComPtr<IDWriteTextFormat> textFormat_;
};

}  // namespace cpictures
