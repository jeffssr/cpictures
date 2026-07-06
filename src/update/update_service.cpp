#include "update/update_service.h"

#include <array>
#include <bcrypt.h>
#include <fstream>
#include <iomanip>
#include <sstream>
#include <vector>

#pragma comment(lib, "bcrypt.lib")

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

}  // namespace

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

}  // namespace cpictures
