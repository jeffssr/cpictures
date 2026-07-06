#pragma once

#include <algorithm>

namespace cpictures {

struct SizeI {
    int width = 0;
    int height = 0;
};

struct PointI {
    int x = 0;
    int y = 0;
};

struct RectI {
    int x = 0;
    int y = 0;
    int width = 0;
    int height = 0;
};

inline bool IsValid(SizeI size) {
    return size.width > 0 && size.height > 0;
}

inline SizeI FitImageWindow(SizeI image, SizeI workArea) {
    if (!IsValid(image) || !IsValid(workArea)) {
        return {};
    }
    if (image.width <= workArea.width && image.height <= workArea.height) {
        return image;
    }
    const double scaleX = static_cast<double>(workArea.width) / image.width;
    const double scaleY = static_cast<double>(workArea.height) / image.height;
    const double scale = std::min(scaleX, scaleY);
    return {
        std::max(1, static_cast<int>(image.width * scale + 0.5)),
        std::max(1, static_cast<int>(image.height * scale + 0.5))
    };
}

inline SizeI ScaleImageToFitWorkArea(SizeI image, SizeI workArea) {
    if (!IsValid(image) || !IsValid(workArea)) {
        return {};
    }
    const double scaleX = static_cast<double>(workArea.width) / image.width;
    const double scaleY = static_cast<double>(workArea.height) / image.height;
    const double scale = std::min(scaleX, scaleY);
    return {
        std::max(1, static_cast<int>(image.width * scale + 0.5)),
        std::max(1, static_cast<int>(image.height * scale + 0.5))
    };
}

}  // namespace cpictures
