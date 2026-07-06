#pragma once

#include <cwctype>
#include <string_view>

namespace cpictures {

inline int CompareNatural(std::wstring_view left, std::wstring_view right) {
    size_t i = 0;
    size_t j = 0;
    while (i < left.size() && j < right.size()) {
        const wchar_t lc = left[i];
        const wchar_t rc = right[j];
        if (std::iswdigit(lc) && std::iswdigit(rc)) {
            unsigned long long ln = 0;
            unsigned long long rn = 0;
            while (i < left.size() && std::iswdigit(left[i])) {
                ln = ln * 10 + static_cast<unsigned long long>(left[i] - L'0');
                ++i;
            }
            while (j < right.size() && std::iswdigit(right[j])) {
                rn = rn * 10 + static_cast<unsigned long long>(right[j] - L'0');
                ++j;
            }
            if (ln != rn) {
                return ln < rn ? -1 : 1;
            }
            continue;
        }
        const wchar_t a = static_cast<wchar_t>(std::towlower(lc));
        const wchar_t b = static_cast<wchar_t>(std::towlower(rc));
        if (a != b) {
            return a < b ? -1 : 1;
        }
        ++i;
        ++j;
    }
    if (i == left.size() && j == right.size()) {
        return 0;
    }
    return i == left.size() ? -1 : 1;
}

inline bool NaturalLess(std::wstring_view left, std::wstring_view right) {
    return CompareNatural(left, right) < 0;
}

}  // namespace cpictures
