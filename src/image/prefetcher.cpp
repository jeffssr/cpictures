#include "image/prefetcher.h"

namespace cpictures {

Prefetcher::~Prefetcher() {
    Stop();
}

void Prefetcher::Warm(const std::filesystem::path& first, const std::filesystem::path& second) {
    Stop();
    {
        std::lock_guard lock(mutex_);
        stopped_ = false;
        cache_.clear();
    }

    workers_.emplace_back([this, first] {
        DecodeOne(first);
    });
    if (second != first) {
        workers_.emplace_back([this, second] {
            DecodeOne(second);
        });
    }
}

std::optional<DecodedImage> Prefetcher::Take(const std::filesystem::path& path) {
    std::lock_guard lock(mutex_);
    const std::wstring key = std::filesystem::absolute(path).wstring();
    auto it = cache_.find(key);
    if (it == cache_.end()) {
        return std::nullopt;
    }

    DecodedImage image = std::move(it->second);
    cache_.erase(it);
    return image;
}

void Prefetcher::Stop() {
    {
        std::lock_guard lock(mutex_);
        stopped_ = true;
    }

    for (auto& worker : workers_) {
        if (worker.joinable()) {
            worker.join();
        }
    }
    workers_.clear();
}

void Prefetcher::DecodeOne(std::filesystem::path path) {
    try {
        WicDecoder decoder;
        DecodedImage image = decoder.Decode(path);
        std::lock_guard lock(mutex_);
        if (!stopped_) {
            cache_[std::filesystem::absolute(path).wstring()] = std::move(image);
        }
    } catch (...) {
    }
}

}  // namespace cpictures
