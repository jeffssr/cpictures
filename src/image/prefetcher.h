#pragma once

#include <filesystem>
#include <mutex>
#include <optional>
#include <thread>
#include <unordered_map>
#include <vector>

#include "image/wic_decoder.h"

namespace cpictures {

class Prefetcher {
public:
    ~Prefetcher();

    void Warm(const std::filesystem::path& first, const std::filesystem::path& second);
    std::optional<DecodedImage> Take(const std::filesystem::path& path);
    void Stop();

private:
    void DecodeOne(std::filesystem::path path);

    std::mutex mutex_;
    std::unordered_map<std::wstring, DecodedImage> cache_;
    std::vector<std::thread> workers_;
    bool stopped_ = false;
};

}  // namespace cpictures
