#include "image/prefetcher.h"

namespace cpictures {

Prefetcher::~Prefetcher() {
    Stop();
}

void Prefetcher::EnsureWorkerStarted() {
    if (!worker_.joinable()) {
        worker_ = std::thread([this] {
            RunWorker();
        });
    }
}

void Prefetcher::Cancel() {
    std::lock_guard lock(mutex_);
    stopped_ = false;
    cache_.clear();
    hasPending_ = false;
    ++generation_;
    condition_.notify_one();
}

void Prefetcher::Warm(const std::filesystem::path& first, const std::filesystem::path& second) {
    std::lock_guard lock(mutex_);
    EnsureWorkerStarted();
    stopped_ = false;
    cache_.clear();
    pendingFirst_ = first;
    pendingSecond_ = second;
    hasPending_ = true;
    ++generation_;
    condition_.notify_one();
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
    std::thread worker;
    {
        std::lock_guard lock(mutex_);
        stopped_ = true;
        hasPending_ = false;
        ++generation_;
        worker.swap(worker_);
    }
    condition_.notify_one();

    if (worker.joinable()) {
        worker.join();
    }
}

void Prefetcher::RunWorker() {
    WicDecoder decoder;
    while (true) {
        std::filesystem::path first;
        std::filesystem::path second;
        size_t generation = 0;
        {
            std::unique_lock lock(mutex_);
            condition_.wait(lock, [this] {
                return stopped_ || hasPending_;
            });
            if (stopped_) {
                return;
            }

            first = pendingFirst_;
            second = pendingSecond_;
            generation = generation_;
            hasPending_ = false;
        }

        DecodeOne(first, generation, decoder);
        if (second != first) {
            DecodeOne(second, generation, decoder);
        }
    }
}

void Prefetcher::DecodeOne(const std::filesystem::path& path, size_t generation, WicDecoder& decoder) {
    try {
        DecodedImage image = decoder.Decode(path);
        std::lock_guard lock(mutex_);
        if (!stopped_ && generation_ == generation) {
            cache_[std::filesystem::absolute(path).wstring()] = std::move(image);
        }
    } catch (...) {
    }
}

}  // namespace cpictures
