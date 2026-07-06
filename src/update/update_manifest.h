#pragma once

#include <string>
#include <vector>

namespace cpictures {

struct CodecPackage {
    std::wstring id;
    std::wstring name;
    std::wstring version;
    std::wstring sha256;
    unsigned long long size = 0;
    std::wstring url;
    std::vector<std::wstring> extensions;
};

struct CodecPackageManifest {
    std::wstring version;
    std::vector<CodecPackage> packages;
};

CodecPackageManifest ParseManifest(const std::wstring& jsonText);

}  // namespace cpictures
