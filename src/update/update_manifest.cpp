#include "update/update_manifest.h"

#include <cwctype>

namespace cpictures {
namespace {

void SkipWhitespace(const std::wstring& text, size_t* pos) {
    while (*pos < text.size() && iswspace(text[*pos]) != 0) {
        ++(*pos);
    }
}

bool ConsumeChar(const std::wstring& text, size_t* pos, wchar_t expected) {
    SkipWhitespace(text, pos);
    if (*pos >= text.size() || text[*pos] != expected) {
        return false;
    }
    ++(*pos);
    return true;
}

bool ConsumeLiteral(const std::wstring& text, size_t* pos, const wchar_t* literal) {
    SkipWhitespace(text, pos);
    size_t index = *pos;
    while (*literal != L'\0') {
        if (index >= text.size() || text[index] != *literal) {
            return false;
        }
        ++index;
        ++literal;
    }
    *pos = index;
    return true;
}

bool ParseHexDigit(wchar_t ch, unsigned int* value) {
    if (ch >= L'0' && ch <= L'9') {
        *value = static_cast<unsigned int>(ch - L'0');
        return true;
    }
    if (ch >= L'a' && ch <= L'f') {
        *value = static_cast<unsigned int>(ch - L'a' + 10);
        return true;
    }
    if (ch >= L'A' && ch <= L'F') {
        *value = static_cast<unsigned int>(ch - L'A' + 10);
        return true;
    }
    return false;
}

bool ParseString(const std::wstring& text, size_t* pos, std::wstring* out) {
    SkipWhitespace(text, pos);
    if (*pos >= text.size() || text[*pos] != L'"') {
        return false;
    }

    ++(*pos);
    std::wstring result;
    while (*pos < text.size()) {
        const wchar_t ch = text[*pos];
        ++(*pos);
        if (ch == L'"') {
            *out = std::move(result);
            return true;
        }
        if (ch == L'\\') {
            if (*pos >= text.size()) {
                return false;
            }
            const wchar_t escaped = text[*pos];
            ++(*pos);
            switch (escaped) {
            case L'"':
            case L'\\':
            case L'/':
                result.push_back(escaped);
                break;
            case L'b':
                result.push_back(L'\b');
                break;
            case L'f':
                result.push_back(L'\f');
                break;
            case L'n':
                result.push_back(L'\n');
                break;
            case L'r':
                result.push_back(L'\r');
                break;
            case L't':
                result.push_back(L'\t');
                break;
            case L'u': {
                unsigned int codePoint = 0;
                for (int i = 0; i < 4; ++i) {
                    if (*pos >= text.size()) {
                        return false;
                    }
                    unsigned int hex = 0;
                    if (!ParseHexDigit(text[*pos], &hex)) {
                        return false;
                    }
                    codePoint = (codePoint << 4) | hex;
                    ++(*pos);
                }
                result.push_back(static_cast<wchar_t>(codePoint));
                break;
            }
            default:
                return false;
            }
            continue;
        }
        result.push_back(ch);
    }
    return false;
}

bool ParseUnsignedLongLong(const std::wstring& text, size_t* pos, unsigned long long* out) {
    SkipWhitespace(text, pos);
    const size_t start = *pos;
    unsigned long long value = 0;
    while (*pos < text.size() && iswdigit(text[*pos]) != 0) {
        const unsigned int digit = static_cast<unsigned int>(text[*pos] - L'0');
        constexpr unsigned long long kMaxValue = ~0ULL;
        if (value > (kMaxValue - digit) / 10) {
            return false;
        }
        value = (value * 10) + digit;
        ++(*pos);
    }
    if (start == *pos) {
        return false;
    }
    *out = value;
    return true;
}

void SkipValue(const std::wstring& text, size_t* pos);

void SkipArray(const std::wstring& text, size_t* pos) {
    if (!ConsumeChar(text, pos, L'[')) {
        return;
    }
    SkipWhitespace(text, pos);
    if (*pos < text.size() && text[*pos] == L']') {
        ++(*pos);
        return;
    }
    while (*pos < text.size()) {
        SkipValue(text, pos);
        SkipWhitespace(text, pos);
        if (*pos < text.size() && text[*pos] == L',') {
            ++(*pos);
            continue;
        }
        if (*pos < text.size() && text[*pos] == L']') {
            ++(*pos);
        }
        return;
    }
}

void SkipObject(const std::wstring& text, size_t* pos) {
    if (!ConsumeChar(text, pos, L'{')) {
        return;
    }
    SkipWhitespace(text, pos);
    if (*pos < text.size() && text[*pos] == L'}') {
        ++(*pos);
        return;
    }
    while (*pos < text.size()) {
        std::wstring key;
        if (!ParseString(text, pos, &key)) {
            return;
        }
        if (!ConsumeChar(text, pos, L':')) {
            return;
        }
        SkipValue(text, pos);
        SkipWhitespace(text, pos);
        if (*pos < text.size() && text[*pos] == L',') {
            ++(*pos);
            continue;
        }
        if (*pos < text.size() && text[*pos] == L'}') {
            ++(*pos);
        }
        return;
    }
}

void SkipValue(const std::wstring& text, size_t* pos) {
    SkipWhitespace(text, pos);
    if (*pos >= text.size()) {
        return;
    }
    switch (text[*pos]) {
    case L'{':
        SkipObject(text, pos);
        return;
    case L'[':
        SkipArray(text, pos);
        return;
    case L'"': {
        std::wstring ignored;
        ParseString(text, pos, &ignored);
        return;
    }
    default:
        while (*pos < text.size()) {
            const wchar_t ch = text[*pos];
            if (ch == L',' || ch == L']' || ch == L'}' || iswspace(ch) != 0) {
                return;
            }
            ++(*pos);
        }
        return;
    }
}

bool ParseExtensions(const std::wstring& text, size_t* pos, std::vector<std::wstring>* extensions) {
    if (!ConsumeChar(text, pos, L'[')) {
        return false;
    }
    SkipWhitespace(text, pos);
    if (*pos < text.size() && text[*pos] == L']') {
        ++(*pos);
        return true;
    }

    while (*pos < text.size()) {
        std::wstring extension;
        if (!ParseString(text, pos, &extension)) {
            return false;
        }
        extensions->push_back(std::move(extension));
        SkipWhitespace(text, pos);
        if (*pos < text.size() && text[*pos] == L',') {
            ++(*pos);
            continue;
        }
        if (*pos < text.size() && text[*pos] == L']') {
            ++(*pos);
            return true;
        }
        return false;
    }
    return false;
}

bool ParsePackage(const std::wstring& text, size_t* pos, CodecPackage* package) {
    if (!ConsumeChar(text, pos, L'{')) {
        return false;
    }
    SkipWhitespace(text, pos);
    if (*pos < text.size() && text[*pos] == L'}') {
        ++(*pos);
        return true;
    }

    while (*pos < text.size()) {
        std::wstring key;
        if (!ParseString(text, pos, &key) || !ConsumeChar(text, pos, L':')) {
            return false;
        }

        if (key == L"id") {
            if (!ParseString(text, pos, &package->id)) {
                return false;
            }
        } else if (key == L"name") {
            if (!ParseString(text, pos, &package->name)) {
                return false;
            }
        } else if (key == L"version") {
            if (!ParseString(text, pos, &package->version)) {
                return false;
            }
        } else if (key == L"sha256") {
            if (!ParseString(text, pos, &package->sha256)) {
                return false;
            }
        } else if (key == L"size") {
            if (!ParseUnsignedLongLong(text, pos, &package->size)) {
                return false;
            }
        } else if (key == L"url") {
            if (!ParseString(text, pos, &package->url)) {
                return false;
            }
        } else if (key == L"extensions") {
            if (!ParseExtensions(text, pos, &package->extensions)) {
                return false;
            }
        } else {
            SkipValue(text, pos);
        }

        SkipWhitespace(text, pos);
        if (*pos < text.size() && text[*pos] == L',') {
            ++(*pos);
            continue;
        }
        if (*pos < text.size() && text[*pos] == L'}') {
            ++(*pos);
            return true;
        }
        return false;
    }
    return false;
}

bool ParsePackages(const std::wstring& text, size_t* pos, std::vector<CodecPackage>* packages) {
    if (!ConsumeChar(text, pos, L'[')) {
        return false;
    }
    SkipWhitespace(text, pos);
    if (*pos < text.size() && text[*pos] == L']') {
        ++(*pos);
        return true;
    }

    while (*pos < text.size()) {
        CodecPackage package;
        if (!ParsePackage(text, pos, &package)) {
            return false;
        }
        if (!package.id.empty()) {
            packages->push_back(std::move(package));
        }
        SkipWhitespace(text, pos);
        if (*pos < text.size() && text[*pos] == L',') {
            ++(*pos);
            continue;
        }
        if (*pos < text.size() && text[*pos] == L']') {
            ++(*pos);
            return true;
        }
        return false;
    }
    return false;
}

}  // namespace

CodecPackageManifest ParseManifest(const std::wstring& jsonText) {
    CodecPackageManifest manifest;
    size_t pos = 0;
    if (!ConsumeChar(jsonText, &pos, L'{')) {
        return manifest;
    }

    SkipWhitespace(jsonText, &pos);
    if (pos < jsonText.size() && jsonText[pos] == L'}') {
        return manifest;
    }

    while (pos < jsonText.size()) {
        std::wstring key;
        if (!ParseString(jsonText, &pos, &key) || !ConsumeChar(jsonText, &pos, L':')) {
            return CodecPackageManifest{};
        }

        if (key == L"version") {
            if (!ParseString(jsonText, &pos, &manifest.version)) {
                return CodecPackageManifest{};
            }
        } else if (key == L"packages") {
            if (!ParsePackages(jsonText, &pos, &manifest.packages)) {
                return CodecPackageManifest{};
            }
        } else {
            SkipValue(jsonText, &pos);
        }

        SkipWhitespace(jsonText, &pos);
        if (pos < jsonText.size() && jsonText[pos] == L',') {
            ++pos;
            continue;
        }
        if (pos < jsonText.size() && jsonText[pos] == L'}') {
            break;
        }
        return CodecPackageManifest{};
    }

    return manifest;
}

}  // namespace cpictures
