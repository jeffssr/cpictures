#include "cpictures/image_list.h"

#include <algorithm>
#include <stdexcept>

#include "cpictures/natural_sort.h"
#include "cpictures/supported_formats.h"

namespace cpictures {
namespace {

bool IsKnownImagePath(const std::filesystem::path& path) {
    const std::wstring ext = path.extension().wstring();
    return IsCoreSupportedExtension(ext) || IsExtensionCandidate(ext);
}

}  // namespace

ImageList ImageList::LoadFromFile(const std::filesystem::path& path) {
    ImageList list;
    const auto absolute = std::filesystem::absolute(path);
    const auto directory = absolute.parent_path();
    if (!std::filesystem::exists(absolute)) {
        list.files_.push_back(absolute);
        return list;
    }
    for (const auto& entry : std::filesystem::directory_iterator(directory)) {
        if (!entry.is_regular_file()) {
            continue;
        }
        if (IsKnownImagePath(entry.path())) {
            list.files_.push_back(std::filesystem::absolute(entry.path()));
        }
    }
    std::sort(list.files_.begin(), list.files_.end(), [](const auto& left, const auto& right) {
        return NaturalLess(left.filename().wstring(), right.filename().wstring());
    });
    const auto it = std::find(list.files_.begin(), list.files_.end(), absolute);
    if (it != list.files_.end()) {
        list.index_ = static_cast<size_t>(std::distance(list.files_.begin(), it));
    } else {
        list.files_.push_back(absolute);
        list.index_ = list.files_.size() - 1;
    }
    return list;
}

const std::filesystem::path& ImageList::Current() const {
    if (files_.empty()) {
        throw std::runtime_error("ImageList is empty");
    }
    return files_[index_];
}

const std::filesystem::path& ImageList::Next() {
    if (files_.empty()) {
        throw std::runtime_error("ImageList is empty");
    }
    index_ = (index_ + 1) % files_.size();
    return Current();
}

const std::filesystem::path& ImageList::Previous() {
    if (files_.empty()) {
        throw std::runtime_error("ImageList is empty");
    }
    index_ = index_ == 0 ? files_.size() - 1 : index_ - 1;
    return Current();
}

size_t ImageList::Index() const {
    return index_;
}

size_t ImageList::Count() const {
    return files_.size();
}

bool ImageList::Empty() const {
    return files_.empty();
}

}  // namespace cpictures
