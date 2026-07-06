#include "image/prefetcher.h"

namespace cpictures {

Prefetcher::~Prefetcher() {
    Stop();
}

void Prefetcher::Cancel() {
    std::lock_guard lock(mutex_);
    stopped_ = false;
    cache_.clear();
    ++generation_;
}

void Prefetcher::Warm(const std::filesystem::path& first, const std::filesystem::path& second) {
    Cancel();
    const size_t generation = [&] {
        std::lock_guard lock(mutex_);
        return generation_;
    }();

    workers_.emplace_back([this, first, generation] {
        DecodeOne(first, generation);
    });
    if (second != first) {
        workers_.emplace_back([this, second, generation] {
            DecodeOne(second, generation);
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
    std::vector<std::thread> workers;
    {
        std::lock_guard lock(mutex_);
        stopped_ = true;
        ++generation_;
        workers.swap(workers_);
    }

    for (auto& worker : workers) {
        if (worker.joinable()) {
            worker.join();
        }
    }
}

void Prefetcher::DecodeOne(std::filesystem::path path, size_t generation) {
    try {
        WicDecoder decoder;
        DecodedImage image = decoder.Decode(path);
        std::lock_guard lock(mutex_);
        if (!stopped_ && generation_ == generation) {
            cache_[std::filesystem::absolute(path).wstring()] = std::move(image);
        }
    } catch (...) {
    }
}

}  // namespace cpictures
