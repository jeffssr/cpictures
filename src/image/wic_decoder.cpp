#include "image/wic_decoder.h"

#include <limits>
#include <stdexcept>

#include <objbase.h>
#include <wincodec.h>
#include <wrl/client.h>

namespace cpictures {
namespace {

void ThrowIfFailed(HRESULT hr, const char* message) {
    if (FAILED(hr)) {
        throw std::runtime_error(message);
    }
}

}  // namespace

WicDecoder::WicDecoder() {
    const HRESULT hr = CoInitializeEx(nullptr, COINIT_APARTMENTTHREADED);
    ownsComApartment_ = SUCCEEDED(hr);
    if (FAILED(hr) && hr != RPC_E_CHANGED_MODE) {
        throw std::runtime_error("CoInitializeEx failed");
    }
}

WicDecoder::~WicDecoder() {
    if (ownsComApartment_) {
        CoUninitialize();
    }
}

DecodedImage WicDecoder::Decode(const std::filesystem::path& path) const {
    Microsoft::WRL::ComPtr<IWICImagingFactory> factory;
    ThrowIfFailed(
        CoCreateInstance(
            CLSID_WICImagingFactory,
            nullptr,
            CLSCTX_INPROC_SERVER,
            IID_PPV_ARGS(&factory)),
        "CoCreateInstance CLSID_WICImagingFactory failed");

    Microsoft::WRL::ComPtr<IWICBitmapDecoder> decoder;
    ThrowIfFailed(
        factory->CreateDecoderFromFilename(
            path.c_str(),
            nullptr,
            GENERIC_READ,
            WICDecodeMetadataCacheOnDemand,
            &decoder),
        "CreateDecoderFromFilename failed");

    Microsoft::WRL::ComPtr<IWICBitmapFrameDecode> frame;
    ThrowIfFailed(decoder->GetFrame(0, &frame), "GetFrame failed");

    UINT width = 0;
    UINT height = 0;
    ThrowIfFailed(frame->GetSize(&width, &height), "GetSize failed");

    Microsoft::WRL::ComPtr<IWICFormatConverter> converter;
    ThrowIfFailed(factory->CreateFormatConverter(&converter), "CreateFormatConverter failed");
    ThrowIfFailed(
        converter->Initialize(
            frame.Get(),
            GUID_WICPixelFormat32bppPBGRA,
            WICBitmapDitherTypeNone,
            nullptr,
            0.0,
            WICBitmapPaletteTypeCustom),
        "FormatConverter Initialize failed");

    if (width > (std::numeric_limits<UINT>::max)() / 4) {
        throw std::runtime_error("decoded image stride overflow");
    }

    const UINT stride = width * 4;
    const unsigned long long bufferBytes =
        static_cast<unsigned long long>(stride) * static_cast<unsigned long long>(height);
    if (bufferBytes > (std::numeric_limits<size_t>::max)()) {
        throw std::runtime_error("decoded image buffer exceeds size_t");
    }
    if (bufferBytes > (std::numeric_limits<UINT>::max)()) {
        throw std::runtime_error("decoded image buffer exceeds UINT");
    }

    DecodedImage result;
    result.size = {static_cast<int>(width), static_cast<int>(height)};
    result.stride = stride;
    result.bgra.resize(static_cast<size_t>(bufferBytes));
    ThrowIfFailed(
        converter->CopyPixels(
            nullptr,
            result.stride,
            static_cast<UINT>(bufferBytes),
            reinterpret_cast<BYTE*>(result.bgra.data())),
        "CopyPixels failed");
    return result;
}

}  // namespace cpictures
