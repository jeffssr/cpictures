#pragma once

namespace cpictures {

enum class Command {
    PreviousImage,
    NextImage,
    ZoomIn,
    ZoomOut,
    ActualSize,
    FitToScreen,
    ToggleFullscreen,
    RotateLeft,
    RotateRight,
    CopyFile,
    CopyPath,
    InstallOrUpdateFormats
};

}  // namespace cpictures
