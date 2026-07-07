#pragma once

#include <filesystem>
#include <vector>

namespace cpictures {

class ImageList {
public:
    static ImageList LoadFromFile(const std::filesystem::path& path);

    const std::filesystem::path& Current() const;
    const std::filesystem::path& Next();
    const std::filesystem::path& Previous();
    size_t Index() const;
    size_t Count() const;
    bool Empty() const;
    bool CanNavigate() const;

private:
    std::vector<std::filesystem::path> files_;
    size_t index_ = 0;
};

}  // namespace cpictures
