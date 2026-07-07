#include "render/d2d_renderer.h"

#include <cmath>
#include <stdexcept>

#include <d2d1helper.h>
#include <dxgiformat.h>

namespace cpictures {
namespace {

void ThrowIfFailed(HRESULT hr, const char* message) {
    if (FAILED(hr)) {
        throw std::runtime_error(message);
    }
}

}  // namespace

D2DRenderer::D2DRenderer() {
    ThrowIfFailed(
        D2D1CreateFactory(D2D1_FACTORY_TYPE_SINGLE_THREADED, factory_.GetAddressOf()),
        "D2D1CreateFactory failed");
    ThrowIfFailed(
        DWriteCreateFactory(
            DWRITE_FACTORY_TYPE_SHARED,
            __uuidof(IDWriteFactory),
            reinterpret_cast<IUnknown**>(writeFactory_.GetAddressOf())),
        "DWriteCreateFactory failed");
    ThrowIfFailed(
        writeFactory_->CreateTextFormat(
            L"Segoe UI",
            nullptr,
            DWRITE_FONT_WEIGHT_NORMAL,
            DWRITE_FONT_STYLE_NORMAL,
            DWRITE_FONT_STRETCH_NORMAL,
            12.0f,
            L"zh-CN",
            textFormat_.GetAddressOf()),
        "CreateTextFormat failed");
    ThrowIfFailed(
        textFormat_->SetWordWrapping(DWRITE_WORD_WRAPPING_NO_WRAP),
        "SetWordWrapping failed");
}

void D2DRenderer::EnsureDevice(HWND hwnd) {
    if (target_) {
        return;
    }

    RECT client{};
    GetClientRect(hwnd, &client);
    const D2D1_SIZE_U size = D2D1::SizeU(
        static_cast<UINT32>(std::max(1L, client.right - client.left)),
        static_cast<UINT32>(std::max(1L, client.bottom - client.top)));

    ThrowIfFailed(
        factory_->CreateHwndRenderTarget(
            D2D1::RenderTargetProperties(
                D2D1_RENDER_TARGET_TYPE_DEFAULT,
                D2D1::PixelFormat(),
                96.0f,
                96.0f),
            D2D1::HwndRenderTargetProperties(hwnd, size),
            target_.GetAddressOf()),
        "CreateHwndRenderTarget failed");
    ThrowIfFailed(
        target_->CreateSolidColorBrush(
            D2D1::ColorF(1.0f, 1.0f, 1.0f, 0.90f),
            textBrush_.GetAddressOf()),
        "CreateSolidColorBrush text failed");
    ThrowIfFailed(
        target_->CreateSolidColorBrush(
            D2D1::ColorF(0.0f, 0.0f, 0.0f, 0.34f),
            badgeBrush_.GetAddressOf()),
        "CreateSolidColorBrush badge failed");

    CreateBitmap();
}

void D2DRenderer::Resize(HWND hwnd) {
    if (!target_) {
        return;
    }

    RECT client{};
    GetClientRect(hwnd, &client);
    const HRESULT hr = target_->Resize(D2D1::SizeU(
        static_cast<UINT32>(std::max(1L, client.right - client.left)),
        static_cast<UINT32>(std::max(1L, client.bottom - client.top))));
    if (hr == D2DERR_RECREATE_TARGET) {
        DiscardDeviceResources();
        return;
    }
    ThrowIfFailed(
        hr,
        "ID2D1HwndRenderTarget::Resize failed");
}

void D2DRenderer::SetImage(const DecodedImage& image) {
    image_ = image;
    if (target_) {
        CreateBitmap();
    }
}

void D2DRenderer::Render(HWND hwnd, const ViewState& state, const std::wstring& overlayText) {
    EnsureDevice(hwnd);

    target_->BeginDraw();
    target_->SetTransform(D2D1::Matrix3x2F::Identity());
    target_->Clear(D2D1::ColorF(D2D1::ColorF::Black));

    const D2D1_SIZE_F clientSize = target_->GetSize();

    if (bitmap_) {
        const D2D1_SIZE_F imageSize = bitmap_->GetSize();
        float scale = static_cast<float>(state.zoom);
        if (scale <= 0.0f) {
            scale = 1.0f;
        }

        const float drawWidth = imageSize.width * scale;
        const float drawHeight = imageSize.height * scale;
        const bool exactPixelScale = std::fabs(scale - std::round(scale)) < 0.0001f;
        const auto interpolation =
            exactPixelScale ? D2D1_BITMAP_INTERPOLATION_MODE_NEAREST_NEIGHBOR
                            : D2D1_BITMAP_INTERPOLATION_MODE_LINEAR;
        const D2D1_RECT_F destination = D2D1::RectF(
            (clientSize.width - drawWidth) * 0.5f,
            (clientSize.height - drawHeight) * 0.5f,
            (clientSize.width + drawWidth) * 0.5f,
            (clientSize.height + drawHeight) * 0.5f);
        D2D1_MATRIX_3X2_F originalTransform{};
        target_->GetTransform(&originalTransform);
        const D2D1_POINT_2F center = D2D1::Point2F(clientSize.width * 0.5f, clientSize.height * 0.5f);
        target_->SetTransform(
            D2D1::Matrix3x2F::Rotation(static_cast<float>(state.rotationDegrees), center) * originalTransform);
        target_->DrawBitmap(
            bitmap_.Get(),
            destination,
            1.0f,
            interpolation);
        target_->SetTransform(originalTransform);
    }

    if (state.overlayVisible && !overlayText.empty()) {
        Microsoft::WRL::ComPtr<IDWriteTextLayout> layout;
        ThrowIfFailed(
            writeFactory_->CreateTextLayout(
                overlayText.c_str(),
                static_cast<UINT32>(overlayText.size()),
                textFormat_.Get(),
                4096.0f,
                64.0f,
                layout.GetAddressOf()),
            "CreateTextLayout failed");

        DWRITE_TEXT_METRICS metrics{};
        ThrowIfFailed(layout->GetMetrics(&metrics), "GetMetrics failed");

        constexpr float paddingX = 8.0f;
        constexpr float paddingY = 5.0f;
        constexpr float margin = 8.0f;
        const float badgeWidth = metrics.width + paddingX * 2.0f;
        const float badgeHeight = metrics.height + paddingY * 2.0f;
        const D2D1_RECT_F badgeRect = D2D1::RectF(
            margin,
            clientSize.height - margin - badgeHeight,
            margin + badgeWidth,
            clientSize.height - margin);
        target_->FillRectangle(badgeRect, badgeBrush_.Get());
        target_->DrawTextLayout(
            D2D1::Point2F(badgeRect.left + paddingX, badgeRect.top + paddingY),
            layout.Get(),
            textBrush_.Get(),
            D2D1_DRAW_TEXT_OPTIONS_CLIP);
    }

    const HRESULT hr = target_->EndDraw();
    if (hr == D2DERR_RECREATE_TARGET) {
        DiscardDeviceResources();
        return;
    }
    ThrowIfFailed(hr, "ID2D1RenderTarget::EndDraw failed");
}

void D2DRenderer::CreateBitmap() {
    bitmap_.Reset();
    if (!target_ || image_.bgra.empty()) {
        return;
    }

    ThrowIfFailed(
        target_->CreateBitmap(
            D2D1::SizeU(
                static_cast<UINT32>(image_.size.width),
                static_cast<UINT32>(image_.size.height)),
            image_.bgra.data(),
            image_.stride,
            D2D1::BitmapProperties(
                D2D1::PixelFormat(
                    DXGI_FORMAT_B8G8R8A8_UNORM,
                    D2D1_ALPHA_MODE_PREMULTIPLIED)),
            bitmap_.GetAddressOf()),
        "CreateBitmap failed");
}

void D2DRenderer::DiscardDeviceResources() {
    bitmap_.Reset();
    textBrush_.Reset();
    badgeBrush_.Reset();
    target_.Reset();
}

}  // namespace cpictures
