#include <array>
#include <cstddef>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <iostream>

#include "image/wic_decoder.h"

namespace {
void Expect(bool condition, const char* message) {
    if (!condition) {
        std::cerr << "FAIL: " << message << "\n";
        std::exit(1);
    }
}

struct TempFileGuard {
    explicit TempFileGuard(std::filesystem::path filePath) : path(std::move(filePath)) {}

    ~TempFileGuard() {
        std::error_code error;
        std::filesystem::remove(path, error);
    }

    std::filesystem::path path;
};

void WriteTinyBmp(const std::filesystem::path& path) {
    const std::array<unsigned char, 70> bmp = {
        0x42,0x4D,0x46,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x36,0x00,0x00,0x00,
        0x28,0x00,0x00,0x00,0x02,0x00,0x00,0x00,0x02,0x00,0x00,0x00,0x01,0x00,
        0x20,0x00,0x00,0x00,0x00,0x00,0x10,0x00,0x00,0x00,0x13,0x0B,0x00,0x00,
        0x13,0x0B,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,
        0x00,0x00,0xFF,0xFF,0x00,0xFF,0x00,0xFF,0xFF,0x00,0x00,0xFF,0xFF,0xFF,0xFF,0xFF
    };
    std::ofstream out(path, std::ios::binary);
    out.write(reinterpret_cast<const char*>(bmp.data()), static_cast<std::streamsize>(bmp.size()));
}
}

int main() {
    const auto path = std::filesystem::temp_directory_path() / L"cpictures_tiny.bmp";
    TempFileGuard guard(path);
    WriteTinyBmp(guard.path);
    cpictures::WicDecoder decoder;
    const cpictures::DecodedImage image = decoder.Decode(guard.path);
    Expect(image.size.width == 2, "decoded width");
    Expect(image.size.height == 2, "decoded height");
    Expect(image.stride == 8, "decoded stride");
    Expect(image.bgra.size() == 16, "decoded bgra bytes");

    const std::array<std::byte, 16> expected = {
        std::byte{0xFF}, std::byte{0x00}, std::byte{0x00}, std::byte{0xFF},
        std::byte{0xFF}, std::byte{0xFF}, std::byte{0xFF}, std::byte{0xFF},
        std::byte{0x00}, std::byte{0x00}, std::byte{0xFF}, std::byte{0xFF},
        std::byte{0x00}, std::byte{0xFF}, std::byte{0x00}, std::byte{0xFF}
    };
    Expect(std::equal(image.bgra.begin(), image.bgra.end(), expected.begin()), "decoded bgra content");

    std::cout << "wic tests passed\n";
    return 0;
}
