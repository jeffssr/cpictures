#include <cstdlib>
#include <filesystem>
#include <fstream>
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

namespace {

void WriteTinyFile(const std::filesystem::path& path) {
    std::ofstream out(path, std::ios::binary);
    out << "x";
}

void TestDllExtensionMatching() {
    Expect(cpictures::IsCodecLibraryExtension(L".DLL"), "uppercase dll extension matches");
    Expect(cpictures::IsCodecLibraryExtension(L"Dll"), "dll extension normalizes without dot");
    Expect(!cpictures::IsCodecLibraryExtension(L".txt"), "non-dll extension does not match");
}

}  // namespace

int main() {
    const std::filesystem::path missingDirectory =
        std::filesystem::temp_directory_path() / L"cpictures_missing_codecs";
    cpictures::CodecManager manager(missingDirectory);

    Expect(manager.Discover().empty(), "missing codecs directory yields no plugins");
    Expect(!manager.FindPluginForExtension(L".psd").has_value(), "missing plugin returns no match");
    TestDllExtensionMatching();

    std::cout << "codec manager tests passed\n";
    return 0;
}
