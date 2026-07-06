#pragma once

#include <condition_variable>
#include <filesystem>
#include <mutex>
#include <optional>
#include <thread>
#include <unordered_map>

#include "image/wic_decoder.h"

namespace cpictures {

class Prefetcher {
public:
    ~Prefetcher();

    void Cancel();
    void Warm(const std::filesystem::path& first, const std::filesystem::path& second);
    std::optional<DecodedImage> Take(const std::filesystem::path& path);
    void Stop();

private:
    void EnsureWorkerStarted();
    void RunWorker();
    void DecodeOne(const std::filesystem::path& path, size_t generation, WicDecoder& decoder);

    std::condition_variable condition_;
    std::mutex mutex_;
    std::unordered_map<std::wstring, DecodedImage> cache_;
    std::thread worker_;
    std::filesystem::path pendingFirst_;
    std::filesystem::path pendingSecond_;
    bool hasPending_ = false;
    size_t generation_ = 0;
    bool stopped_ = false;
};

}  // namespace cpictures
