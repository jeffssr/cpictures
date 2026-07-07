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

inline SizeI ScaleImageByZoomToWorkArea(SizeI image, SizeI workArea, double zoom) {
    if (!IsValid(image) || !IsValid(workArea) || zoom <= 0.0) {
        return {};
    }
    const SizeI zoomed{
        std::max(1, static_cast<int>(image.width * zoom + 0.5)),
        std::max(1, static_cast<int>(image.height * zoom + 0.5))
    };
    return FitImageWindow(zoomed, workArea);
}

inline SizeI ActualSizeViewport(SizeI image, SizeI workArea) {
    if (!IsValid(image) || !IsValid(workArea)) {
        return {};
    }
    return {std::min(image.width, workArea.width), std::min(image.height, workArea.height)};
}

inline bool CanPan(SizeI content, SizeI viewport) {
    return IsValid(content) && IsValid(viewport) &&
           (content.width > viewport.width || content.height > viewport.height);
}

inline PointI ClampPanOffset(PointI pan, SizeI content, SizeI viewport) {
    const int maxX = std::max(0, content.width - viewport.width);
    const int maxY = std::max(0, content.height - viewport.height);
    return {std::clamp(pan.x, 0, maxX), std::clamp(pan.y, 0, maxY)};
}

inline SizeI CorrectWindowSizeForClient(SizeI targetClient, SizeI actualClient, SizeI currentWindow) {
    if (!IsValid(targetClient) || !IsValid(actualClient) || !IsValid(currentWindow)) {
        return {};
    }
    return {
        std::max(1, currentWindow.width + (targetClient.width - actualClient.width)),
        std::max(1, currentWindow.height + (targetClient.height - actualClient.height))
    };
}

}  // namespace cpictures
