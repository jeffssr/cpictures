#pragma once

#include <string>
#include <string_view>

namespace cpictures {

inline std::wstring BuildOverlayText(std::wstring_view fileName, int zeroBasedIndex, int totalCount) {
    std::wstring text = std::to_wstring(zeroBasedIndex + 1);
    text += L"/";
    text += std::to_wstring(totalCount);
    text += L"  ";
    text += fileName;
    return text;
}

}  // namespace cpictures
