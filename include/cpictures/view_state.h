#pragma once

namespace cpictures {

enum class FitMode {
    ActualSize,
    FitToScreen
};

struct ViewState {
    double zoom = 1.0;
    int panX = 0;
    int panY = 0;
    int rotationDegrees = 0;
    bool overlayVisible = false;
    bool fullscreen = false;
    FitMode fitMode = FitMode::ActualSize;
};

}  // namespace cpictures
