#include "cpictures/supported_formats.h"

#include <algorithm>
#include <cwctype>

namespace cpictures {

std::wstring NormalizeExtension(std::wstring_view ext) {
    std::wstring value(ext);
    if (!value.empty() && value.front() != L'.') {
        value.insert(value.begin(), L'.');
    }
    std::transform(value.begin(), value.end(), value.begin(), [](wchar_t ch) {
        return static_cast<wchar_t>(std::towlower(ch));
    });
    return value;
}

const std::vector<std::wstring>& CoreSupportedExtensions() {
    static const std::vector<std::wstring> formats{
        L".jpg", L".jpeg", L".png", L".bmp", L".gif", L".tif", L".tiff",
        L".webp", L".heic", L".heif", L".ico"
    };
    return formats;
}

const std::vector<std::wstring>& ExtensionSupportedCandidates() {
    static const std::vector<std::wstring> formats{
        L".avif", L".svg", L".psd", L".psb", L".cr2", L".cr3", L".nef",
        L".arw", L".dng", L".rw2", L".orf", L".raf", L".exr", L".hdr"
    };
    return formats;
}

bool IsCoreSupportedExtension(std::wstring_view ext) {
    const std::wstring normalized = NormalizeExtension(ext);
    const auto& formats = CoreSupportedExtensions();
    return std::find(formats.begin(), formats.end(), normalized) != formats.end();
}

bool IsExtensionCandidate(std::wstring_view ext) {
    const std::wstring normalized = NormalizeExtension(ext);
    const auto& formats = ExtensionSupportedCandidates();
    return std::find(formats.begin(), formats.end(), normalized) != formats.end();
}

}  // namespace cpictures
