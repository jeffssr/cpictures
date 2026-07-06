#include "codecs/codec_manager.h"

#include <algorithm>

#include <windows.h>

#include "codecs/cpictures_codec.h"
#include "cpictures/supported_formats.h"

namespace cpictures {
namespace {

std::vector<std::wstring> SplitExtensions(const wchar_t* value) {
    std::vector<std::wstring> result;
    std::wstring current;
    for (const wchar_t* p = value; p != nullptr && *p != L'\0'; ++p) {
        if (*p == L';') {
            if (!current.empty()) {
                result.push_back(NormalizeExtension(current));
                current.clear();
            }
            continue;
        }
        current.push_back(*p);
    }
    if (!current.empty()) {
        result.push_back(NormalizeExtension(current));
    }
    return result;
}

}  // namespace

CodecManager::CodecManager(std::filesystem::path codecsDirectory)
    : codecsDirectory_(std::move(codecsDirectory)) {}

std::vector<CodecPluginInfo> CodecManager::Discover() const {
    std::vector<CodecPluginInfo> plugins;
    std::error_code error;
    if (!std::filesystem::exists(codecsDirectory_, error) || error) {
        return plugins;
    }

    for (const auto& entry : std::filesystem::directory_iterator(codecsDirectory_, error)) {
        if (error) {
            return {};
        }
        if (!entry.is_regular_file() || entry.path().extension() != L".dll") {
            continue;
        }

        const HMODULE module = LoadLibraryW(entry.path().c_str());
        if (module == nullptr) {
            continue;
        }

        const auto infoFn = reinterpret_cast<cpictures_codec_info_fn>(
            GetProcAddress(module, "cpictures_codec_info"));
        if (infoFn != nullptr) {
            cpictures_codec_info info{};
            if (infoFn(&info) == 0 && info.abi_version == 1) {
                plugins.push_back(CodecPluginInfo{
                    entry.path(),
                    info.name != nullptr ? info.name : L"",
                    info.version != nullptr ? info.version : L"",
                    SplitExtensions(info.extensions)
                });
            }
        }

        FreeLibrary(module);
    }

    return plugins;
}

std::optional<CodecPluginInfo> CodecManager::FindPluginForExtension(std::wstring extension) const {
    const std::wstring normalized = NormalizeExtension(extension);
    for (const auto& plugin : Discover()) {
        if (std::find(plugin.extensions.begin(), plugin.extensions.end(), normalized) != plugin.extensions.end()) {
            return plugin;
        }
    }
    return std::nullopt;
}

}  // namespace cpictures
