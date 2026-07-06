#pragma once

#include <cstddef>
#include <filesystem>
#include <vector>

#include "cpictures/geometry.h"

namespace cpictures {

struct DecodedImage {
    SizeI size;
    unsigned stride = 0;
    std::vector<std::byte> bgra;
};

class WicDecoder {
public:
    WicDecoder();
    ~WicDecoder();
    WicDecoder(const WicDecoder&) = delete;
    WicDecoder& operator=(const WicDecoder&) = delete;

    DecodedImage Decode(const std::filesystem::path& path) const;

private:
    bool ownsComApartment_ = false;
};

}  // namespace cpictures
