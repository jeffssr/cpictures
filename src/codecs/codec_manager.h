#pragma once

#include <filesystem>
#include <optional>
#include <string>
#include <vector>

namespace cpictures {

struct CodecPluginInfo {
    std::filesystem::path path;
    std::wstring name;
    std::wstring version;
    std::vector<std::wstring> extensions;
};

class CodecManager {
public:
    explicit CodecManager(std::filesystem::path codecsDirectory);
    std::vector<CodecPluginInfo> Discover() const;
    std::optional<CodecPluginInfo> FindPluginForExtension(std::wstring extension) const;

private:
    std::filesystem::path codecsDirectory_;
};

}  // namespace cpictures
