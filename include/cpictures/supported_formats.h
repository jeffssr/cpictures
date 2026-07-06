#pragma once

#include <string>
#include <string_view>
#include <vector>

namespace cpictures {

std::wstring NormalizeExtension(std::wstring_view ext);
bool IsCoreSupportedExtension(std::wstring_view ext);
bool IsExtensionCandidate(std::wstring_view ext);
const std::vector<std::wstring>& CoreSupportedExtensions();
const std::vector<std::wstring>& ExtensionSupportedCandidates();

}  // namespace cpictures
