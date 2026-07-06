#include <cstdlib>
#include <filesystem>
#include <iostream>

#include "codecs/codec_manager.h"

namespace {

void Expect(bool condition, const char* message) {
    if (!condition) {
        std::cerr << "FAIL: " << message << "\n";
        std::exit(1);
    }
}

}  // namespace

int main() {
    const std::filesystem::path missingDirectory =
        std::filesystem::temp_directory_path() / L"cpictures_missing_codecs";
    cpictures::CodecManager manager(missingDirectory);

    Expect(manager.Discover().empty(), "missing codecs directory yields no plugins");
    Expect(!manager.FindPluginForExtension(L".psd").has_value(), "missing plugin returns no match");

    std::cout << "codec manager tests passed\n";
    return 0;
}
