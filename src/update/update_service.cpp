#include "update/update_service.h"

#include <array>
#include <bcrypt.h>
#include <cwctype>
#include <fstream>
#include <iomanip>
#include <optional>
#include <shellapi.h>
#include <sstream>
#include <utility>
#include <vector>
#include <winhttp.h>

#pragma comment(lib, "bcrypt.lib")
#pragma comment(lib, "winhttp.lib")

namespace cpictures {
namespace {

struct ScopedAlgorithm {
    BCRYPT_ALG_HANDLE handle = nullptr;

    ~ScopedAlgorithm() {
        if (handle != nullptr) {
            BCryptCloseAlgorithmProvider(handle, 0);
        }
    }
};

struct ScopedHash {
    BCRYPT_HASH_HANDLE handle = nullptr;

    ~ScopedHash() {
        if (handle != nullptr) {
            BCryptDestroyHash(handle);
        }
    }
};

struct ScopedInternetHandle {
    HINTERNET handle = nullptr;

    ScopedInternetHandle() = default;
    explicit ScopedInternetHandle(HINTERNET value) : handle(value) {}
    ScopedInternetHandle(const ScopedInternetHandle&) = delete;
    ScopedInternetHandle& operator=(const ScopedInternetHandle&) = delete;
    ScopedInternetHandle(ScopedInternetHandle&& other) noexcept : handle(other.handle) {
        other.handle = nullptr;
    }
    ScopedInternetHandle& operator=(ScopedInternetHandle&& other) noexcept {
        if (this != &other) {
            if (handle != nullptr) {
                WinHttpCloseHandle(handle);
            }
            handle = other.handle;
            other.handle = nullptr;
        }
        return *this;
    }

    ~ScopedInternetHandle() {
        if (handle != nullptr) {
            WinHttpCloseHandle(handle);
        }
    }
};

struct HttpRequestHandles {
    ScopedInternetHandle session;
    ScopedInternetHandle connection;
    ScopedInternetHandle request;
};

struct ScopedWaitCursor {
    HCURSOR previous = nullptr;

    ScopedWaitCursor() {
        previous = SetCursor(LoadCursorW(nullptr, IDC_WAIT));
    }

    ~ScopedWaitCursor() {
        SetCursor(previous);
    }
};

bool QueryDword(BCRYPT_ALG_HANDLE algorithm, const wchar_t* property, DWORD* value) {
    DWORD cbResult = 0;
    return BCRYPT_SUCCESS(BCryptGetProperty(
               algorithm,
               property,
               reinterpret_cast<PUCHAR>(value),
               sizeof(*value),
               &cbResult,
               0)) &&
           cbResult == sizeof(*value);
}

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

bool ParseJsonString(const std::wstring& text, size_t* pos, std::wstring* out) {
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

void SkipJsonValue(const std::wstring& text, size_t* pos);

void SkipJsonArray(const std::wstring& text, size_t* pos) {
    if (!ConsumeChar(text, pos, L'[')) {
        return;
    }
    SkipWhitespace(text, pos);
    if (*pos < text.size() && text[*pos] == L']') {
        ++(*pos);
        return;
    }
    while (*pos < text.size()) {
        SkipJsonValue(text, pos);
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

void SkipJsonObject(const std::wstring& text, size_t* pos) {
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
        if (!ParseJsonString(text, pos, &key) || !ConsumeChar(text, pos, L':')) {
            return;
        }
        SkipJsonValue(text, pos);
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

void SkipJsonValue(const std::wstring& text, size_t* pos) {
    SkipWhitespace(text, pos);
    if (*pos >= text.size()) {
        return;
    }
    switch (text[*pos]) {
    case L'{':
        SkipJsonObject(text, pos);
        return;
    case L'[':
        SkipJsonArray(text, pos);
        return;
    case L'"': {
        std::wstring ignored;
        ParseJsonString(text, pos, &ignored);
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

std::wstring ToLower(std::wstring value) {
    for (wchar_t& ch : value) {
        ch = static_cast<wchar_t>(towlower(ch));
    }
    return value;
}

bool EndsWith(std::wstring_view value, std::wstring_view suffix) {
    return value.size() >= suffix.size() &&
           value.compare(value.size() - suffix.size(), suffix.size(), suffix) == 0;
}

bool StartsWith(std::wstring_view value, std::wstring_view prefix) {
    return value.size() >= prefix.size() && value.compare(0, prefix.size(), prefix) == 0;
}

bool IsPreferredInstaller(const ReleaseAsset& asset) {
    const std::wstring lowerName = ToLower(asset.name);
    return StartsWith(lowerName, L"cpictures-") && EndsWith(lowerName, L".msi") &&
           !asset.downloadUrl.empty();
}

bool IsInstaller(const ReleaseAsset& asset) {
    const std::wstring lowerName = ToLower(asset.name);
    return EndsWith(lowerName, L".msi") && !asset.downloadUrl.empty();
}

bool ParseReleaseAsset(const std::wstring& text, size_t* pos, ReleaseAsset* asset) {
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
        if (!ParseJsonString(text, pos, &key) || !ConsumeChar(text, pos, L':')) {
            return false;
        }

        if (key == L"name") {
            if (!ParseJsonString(text, pos, &asset->name)) {
                return false;
            }
        } else if (key == L"browser_download_url") {
            if (!ParseJsonString(text, pos, &asset->downloadUrl)) {
                return false;
            }
        } else if (key == L"digest") {
            if (!ParseJsonString(text, pos, &asset->digest)) {
                return false;
            }
        } else if (key == L"size") {
            if (!ParseUnsignedLongLong(text, pos, &asset->size)) {
                return false;
            }
        } else {
            SkipJsonValue(text, pos);
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

bool ParseReleaseAssets(const std::wstring& text, size_t* pos, ReleaseAsset* selected) {
    if (!ConsumeChar(text, pos, L'[')) {
        return false;
    }
    SkipWhitespace(text, pos);
    if (*pos < text.size() && text[*pos] == L']') {
        ++(*pos);
        return true;
    }

    ReleaseAsset fallback;
    while (*pos < text.size()) {
        ReleaseAsset asset;
        if (!ParseReleaseAsset(text, pos, &asset)) {
            return false;
        }
        if (IsPreferredInstaller(asset)) {
            *selected = std::move(asset);
        } else if (selected->name.empty() && fallback.name.empty() && IsInstaller(asset)) {
            fallback = std::move(asset);
        }

        SkipWhitespace(text, pos);
        if (*pos < text.size() && text[*pos] == L',') {
            ++(*pos);
            continue;
        }
        if (*pos < text.size() && text[*pos] == L']') {
            ++(*pos);
            if (selected->name.empty()) {
                *selected = std::move(fallback);
            }
            return true;
        }
        return false;
    }
    return false;
}

std::wstring_view TrimVersionPrefix(std::wstring_view value) {
    while (!value.empty() && iswspace(value.front()) != 0) {
        value.remove_prefix(1);
    }
    if (!value.empty() && (value.front() == L'v' || value.front() == L'V')) {
        value.remove_prefix(1);
    }
    return value;
}

unsigned long long ReadVersionPart(std::wstring_view value, size_t* pos) {
    unsigned long long result = 0;
    while (*pos < value.size() && value[*pos] >= L'0' && value[*pos] <= L'9') {
        const unsigned int digit = static_cast<unsigned int>(value[*pos] - L'0');
        constexpr unsigned long long kMaxValue = ~0ULL;
        if (result > (kMaxValue - digit) / 10) {
            return kMaxValue;
        }
        result = (result * 10) + digit;
        ++(*pos);
    }
    return result;
}

std::wstring Utf8ToWide(const std::string& value) {
    if (value.empty()) {
        return L"";
    }
    const int length = MultiByteToWideChar(
        CP_UTF8, MB_ERR_INVALID_CHARS, value.data(), static_cast<int>(value.size()), nullptr, 0);
    if (length <= 0) {
        return L"";
    }
    std::wstring result(static_cast<size_t>(length), L'\0');
    MultiByteToWideChar(
        CP_UTF8, MB_ERR_INVALID_CHARS, value.data(), static_cast<int>(value.size()), result.data(), length);
    return result;
}

std::string WideToUtf8(const std::wstring& value) {
    if (value.empty()) {
        return "";
    }
    const int length = WideCharToMultiByte(
        CP_UTF8, 0, value.data(), static_cast<int>(value.size()), nullptr, 0, nullptr, nullptr);
    if (length <= 0) {
        return "";
    }
    std::string result(static_cast<size_t>(length), '\0');
    WideCharToMultiByte(
        CP_UTF8, 0, value.data(), static_cast<int>(value.size()), result.data(), length, nullptr, nullptr);
    return result;
}

bool CrackUrl(const std::wstring& url, URL_COMPONENTS* components, std::wstring* host, std::wstring* path) {
    *host = std::wstring(256, L'\0');
    *path = std::wstring(2048, L'\0');

    *components = {};
    components->dwStructSize = sizeof(*components);
    components->lpszHostName = host->data();
    components->dwHostNameLength = static_cast<DWORD>(host->size());
    components->lpszUrlPath = path->data();
    components->dwUrlPathLength = static_cast<DWORD>(path->size());
    components->dwSchemeLength = static_cast<DWORD>(-1);

    if (!WinHttpCrackUrl(url.c_str(), static_cast<DWORD>(url.size()), 0, components)) {
        return false;
    }

    host->resize(components->dwHostNameLength);
    path->resize(components->dwUrlPathLength);
    return components->nScheme == INTERNET_SCHEME_HTTP || components->nScheme == INTERNET_SCHEME_HTTPS;
}

HttpRequestHandles OpenHttpRequest(const std::wstring& url, const wchar_t* method) {
    URL_COMPONENTS components{};
    std::wstring host;
    std::wstring path;
    if (!CrackUrl(url, &components, &host, &path)) {
        return {};
    }

    ScopedInternetHandle session{WinHttpOpen(
        L"cpictures/1.0",
        WINHTTP_ACCESS_TYPE_DEFAULT_PROXY,
        WINHTTP_NO_PROXY_NAME,
        WINHTTP_NO_PROXY_BYPASS,
        0)};
    if (session.handle == nullptr) {
        return {};
    }

    ScopedInternetHandle connection{WinHttpConnect(session.handle, host.c_str(), components.nPort, 0)};
    if (connection.handle == nullptr) {
        return {};
    }

    const DWORD flags = components.nScheme == INTERNET_SCHEME_HTTPS ? WINHTTP_FLAG_SECURE : 0;
    ScopedInternetHandle request{WinHttpOpenRequest(
        connection.handle,
        method,
        path.c_str(),
        nullptr,
        WINHTTP_NO_REFERER,
        WINHTTP_DEFAULT_ACCEPT_TYPES,
        flags)};
    if (request.handle == nullptr) {
        return {};
    }

    const DWORD redirectPolicy = WINHTTP_OPTION_REDIRECT_POLICY_ALWAYS;
    WinHttpSetOption(
        request.handle,
        WINHTTP_OPTION_REDIRECT_POLICY,
        const_cast<DWORD*>(&redirectPolicy),
        sizeof(redirectPolicy));

    return {std::move(session), std::move(connection), std::move(request)};
}

bool SendHttpRequest(HINTERNET request, const wchar_t* headers) {
    if (!WinHttpSendRequest(
            request,
            headers,
            headers != nullptr ? static_cast<DWORD>(-1L) : 0,
            WINHTTP_NO_REQUEST_DATA,
            0,
            0,
            0)) {
        return false;
    }
    return WinHttpReceiveResponse(request, nullptr) != FALSE;
}

DWORD QueryStatusCode(HINTERNET request) {
    DWORD status = 0;
    DWORD size = sizeof(status);
    if (!WinHttpQueryHeaders(
            request,
            WINHTTP_QUERY_STATUS_CODE | WINHTTP_QUERY_FLAG_NUMBER,
            WINHTTP_HEADER_NAME_BY_INDEX,
            &status,
            &size,
            WINHTTP_NO_HEADER_INDEX)) {
        return 0;
    }
    return status;
}

bool ReadHttpBytes(HINTERNET request, std::vector<unsigned char>* bytes, std::ofstream* file) {
    while (true) {
        DWORD available = 0;
        if (!WinHttpQueryDataAvailable(request, &available)) {
            return false;
        }
        if (available == 0) {
            return true;
        }

        std::vector<unsigned char> buffer(available);
        DWORD read = 0;
        if (!WinHttpReadData(request, buffer.data(), available, &read)) {
            return false;
        }
        if (read == 0) {
            return true;
        }
        if (file != nullptr) {
            file->write(reinterpret_cast<const char*>(buffer.data()), read);
            if (!*file) {
                return false;
            }
        } else {
            bytes->insert(bytes->end(), buffer.begin(), buffer.begin() + read);
        }
    }
}

std::optional<std::wstring> HttpGetText(const wchar_t* url) {
    HttpRequestHandles handles = OpenHttpRequest(url, L"GET");
    if (handles.request.handle == nullptr) {
        return std::nullopt;
    }

    constexpr wchar_t kHeaders[] =
        L"Accept: application/vnd.github+json\r\n"
        L"User-Agent: cpictures\r\n";
    if (!SendHttpRequest(handles.request.handle, kHeaders) ||
        QueryStatusCode(handles.request.handle) != 200) {
        return std::nullopt;
    }

    std::vector<unsigned char> bytes;
    if (!ReadHttpBytes(handles.request.handle, &bytes, nullptr)) {
        return std::nullopt;
    }

    const std::string utf8(bytes.begin(), bytes.end());
    std::wstring text = Utf8ToWide(utf8);
    if (text.empty() && !utf8.empty()) {
        return std::nullopt;
    }
    return text;
}

bool DownloadFile(const std::wstring& url, const std::filesystem::path& path) {
    HttpRequestHandles handles = OpenHttpRequest(url, L"GET");
    if (handles.request.handle == nullptr) {
        return false;
    }

    constexpr wchar_t kHeaders[] = L"User-Agent: cpictures\r\n";
    if (!SendHttpRequest(handles.request.handle, kHeaders)) {
        return false;
    }

    const DWORD status = QueryStatusCode(handles.request.handle);
    if (status < 200 || status >= 300) {
        return false;
    }

    std::filesystem::create_directories(path.parent_path());
    std::ofstream file(path, std::ios::binary);
    if (!file) {
        return false;
    }
    std::vector<unsigned char> unused;
    return ReadHttpBytes(handles.request.handle, &unused, &file);
}

std::wstring CurrentVersion() {
#ifdef CPICTURES_VERSION
    return CPICTURES_VERSION;
#else
    return L"0.1.0";
#endif
}

std::filesystem::path UpdateDownloadPath(const std::wstring& fileName) {
    wchar_t buffer[MAX_PATH]{};
    const DWORD length = GetTempPathW(static_cast<DWORD>(std::size(buffer)), buffer);
    const std::filesystem::path directory = length > 0
        ? std::filesystem::path(buffer) / L"cpictures-update"
        : std::filesystem::temp_directory_path() / L"cpictures-update";
    return directory / fileName;
}

bool LaunchInstaller(const std::filesystem::path& path) {
    const std::wstring parameters = L"/i \"" + path.wstring() + L"\"";
    const HINSTANCE result = ShellExecuteW(
        nullptr,
        L"open",
        L"msiexec.exe",
        parameters.c_str(),
        nullptr,
        SW_SHOWNORMAL);
    return reinterpret_cast<INT_PTR>(result) > 32;
}

std::wstring NewVersionMessage(const std::wstring& tagName) {
    return L"\x53D1\x73B0\x65B0\x7248\x672C " + tagName +
           L"\xFF0C\x662F\x5426\x4E0B\x8F7D\x5E76\x5B89\x88C5\xFF1F";
}

}  // namespace

int CompareVersions(std::wstring_view left, std::wstring_view right) {
    left = TrimVersionPrefix(left);
    right = TrimVersionPrefix(right);

    size_t leftPos = 0;
    size_t rightPos = 0;
    while (leftPos < left.size() || rightPos < right.size()) {
        const unsigned long long leftPart = ReadVersionPart(left, &leftPos);
        const unsigned long long rightPart = ReadVersionPart(right, &rightPos);
        if (leftPart < rightPart) {
            return -1;
        }
        if (leftPart > rightPart) {
            return 1;
        }
        if (leftPos < left.size() && left[leftPos] == L'.') {
            ++leftPos;
        }
        if (rightPos < right.size() && right[rightPos] == L'.') {
            ++rightPos;
        }
        while (leftPos < left.size() && left[leftPos] != L'.' && !iswdigit(left[leftPos])) {
            ++leftPos;
        }
        while (rightPos < right.size() && right[rightPos] != L'.' && !iswdigit(right[rightPos])) {
            ++rightPos;
        }
    }
    return 0;
}

bool IsNewerVersion(std::wstring_view remote, std::wstring_view local) {
    return CompareVersions(remote, local) > 0;
}

ReleaseInfo ParseLatestRelease(const std::wstring& jsonText) {
    ReleaseInfo release;
    size_t pos = 0;
    if (!ConsumeChar(jsonText, &pos, L'{')) {
        return release;
    }

    SkipWhitespace(jsonText, &pos);
    if (pos < jsonText.size() && jsonText[pos] == L'}') {
        return release;
    }

    while (pos < jsonText.size()) {
        std::wstring key;
        if (!ParseJsonString(jsonText, &pos, &key) || !ConsumeChar(jsonText, &pos, L':')) {
            return ReleaseInfo{};
        }

        if (key == L"tag_name") {
            if (!ParseJsonString(jsonText, &pos, &release.tagName)) {
                return ReleaseInfo{};
            }
        } else if (key == L"assets") {
            if (!ParseReleaseAssets(jsonText, &pos, &release.installer)) {
                return ReleaseInfo{};
            }
        } else {
            SkipJsonValue(jsonText, &pos);
        }

        SkipWhitespace(jsonText, &pos);
        if (pos < jsonText.size() && jsonText[pos] == L',') {
            ++pos;
            continue;
        }
        if (pos < jsonText.size() && jsonText[pos] == L'}') {
            break;
        }
        return ReleaseInfo{};
    }

    return release;
}

std::wstring ComputeSha256(const std::filesystem::path& path) {
    std::ifstream input(path, std::ios::binary);
    if (!input) {
        return L"";
    }

    ScopedAlgorithm algorithm;
    if (!BCRYPT_SUCCESS(BCryptOpenAlgorithmProvider(&algorithm.handle, BCRYPT_SHA256_ALGORITHM, nullptr, 0))) {
        return L"";
    }

    DWORD objectLength = 0;
    DWORD hashLength = 0;
    if (!QueryDword(algorithm.handle, BCRYPT_OBJECT_LENGTH, &objectLength) ||
        !QueryDword(algorithm.handle, BCRYPT_HASH_LENGTH, &hashLength)) {
        return L"";
    }

    std::vector<unsigned char> objectBuffer(objectLength);
    std::vector<unsigned char> hashBuffer(hashLength);

    ScopedHash hash;
    if (!BCRYPT_SUCCESS(BCryptCreateHash(
            algorithm.handle,
            &hash.handle,
            objectBuffer.data(),
            static_cast<ULONG>(objectBuffer.size()),
            nullptr,
            0,
            0))) {
        return L"";
    }

    std::array<char, 64 * 1024> chunk{};
    while (input.good()) {
        input.read(chunk.data(), static_cast<std::streamsize>(chunk.size()));
        const std::streamsize count = input.gcount();
        if (count <= 0) {
            break;
        }
        if (!BCRYPT_SUCCESS(BCryptHashData(hash.handle,
                           reinterpret_cast<PUCHAR>(chunk.data()),
                           static_cast<ULONG>(count),
                           0))) {
            return L"";
        }
    }

    if (input.bad()) {
        return L"";
    }

    if (!BCRYPT_SUCCESS(BCryptFinishHash(
            hash.handle, hashBuffer.data(), static_cast<ULONG>(hashBuffer.size()), 0))) {
        return L"";
    }

    std::wstringstream out;
    out << std::hex << std::setfill(L'0');
    for (unsigned char byte : hashBuffer) {
        out << std::setw(2) << static_cast<unsigned int>(byte);
    }
    return out.str();
}

void ShowFormatSupportDialog(HWND owner) {
    MessageBoxW(
        owner,
        L"\x683C\x5F0F\x652F\x6301\x6269\x5C55\x5C06\x5728\x4F60\x786E\x8BA4\x540E\x624D\x5F00\x59CB\x4E0B\x8F7D\x3002\n\n"
        L"\x4E0B\x8F7D\x524D\x4F1A\x663E\x793A\x7EC4\x4EF6\x540D\x79F0\x3001\x7248\x672C\x3001\x5927\x5C0F\x548C\x6765\x6E90\xFF0C"
        L"\x5E76\x5728\x5B89\x88C5\x524D\x6821\x9A8C SHA-256\x3002\n\n"
        L"\x5F53\x524D\x7248\x672C\x53EA\x63D0\x4F9B\x5165\x53E3\x8BF4\x660E\xFF0C\x4E0D\x4F1A\x81EA\x52A8\x4E0B\x8F7D\x6216\x66F4\x65B0\x3002",
        L"\x5B89\x88C5/\x66F4\x65B0\x683C\x5F0F\x652F\x6301",
        MB_OK | MB_ICONINFORMATION);
}

void CheckForUpdates(HWND owner) {
    ScopedWaitCursor waitCursor;

    const auto json = HttpGetText(L"https://api.github.com/repos/jeffssr/cpictures/releases/latest");
    if (!json) {
        MessageBoxW(owner, L"\x68C0\x67E5\x66F4\x65B0\x5931\x8D25\x3002", L"cpictures", MB_OK | MB_ICONERROR);
        return;
    }

    const ReleaseInfo release = ParseLatestRelease(*json);
    if (release.tagName.empty()) {
        MessageBoxW(owner, L"\x68C0\x67E5\x66F4\x65B0\x5931\x8D25\x3002", L"cpictures", MB_OK | MB_ICONERROR);
        return;
    }

    if (!IsNewerVersion(release.tagName, CurrentVersion())) {
        MessageBoxW(owner, L"\x5DF2\x662F\x6700\x65B0\x7248\x672C\x3002", L"cpictures", MB_OK | MB_ICONINFORMATION);
        return;
    }

    if (release.installer.downloadUrl.empty()) {
        MessageBoxW(owner, L"\x672A\x627E\x5230\x5B89\x88C5\x5305\x3002", L"cpictures", MB_OK | MB_ICONERROR);
        return;
    }

    const std::wstring question = NewVersionMessage(release.tagName);
    const int answer = MessageBoxW(owner, question.c_str(), L"cpictures", MB_YESNO | MB_ICONQUESTION);
    if (answer != IDYES) {
        return;
    }

    const std::filesystem::path downloadPath = UpdateDownloadPath(release.installer.name);
    if (!DownloadFile(release.installer.downloadUrl, downloadPath)) {
        MessageBoxW(owner, L"\x4E0B\x8F7D\x5931\x8D25\x3002", L"cpictures", MB_OK | MB_ICONERROR);
        return;
    }

    if (!LaunchInstaller(downloadPath)) {
        MessageBoxW(owner, L"\x542F\x52A8\x5B89\x88C5\x5931\x8D25\x3002", L"cpictures", MB_OK | MB_ICONERROR);
    }
}

}  // namespace cpictures
