#pragma once

#include <filesystem>
#include <string>

#include "cpictures/geometry.h"

namespace cpictures {

struct ImageDocumentMetadata {
    std::filesystem::path path;
    std::wstring fileName;
    std::wstring extension;
    SizeI pixelSize;
    int frameCount = 1;
    bool animated = false;
};

}  // namespace cpictures
