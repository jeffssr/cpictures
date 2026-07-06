#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <iostream>

#include "update/update_manifest.h"
#include "update/update_service.h"

namespace {

void Expect(bool condition, const char* message) {
    if (!condition) {
        std::cerr << "FAIL: " << message << "\n";
        std::exit(1);
    }
}

void ExpectWideEqual(const std::wstring& actual, const std::wstring& expected, const char* message) {
    if (actual == expected) {
        return;
    }
    std::cerr << "FAIL: " << message << "\nactual codepoints:";
    for (wchar_t ch : actual) {
        std::cerr << " " << static_cast<unsigned int>(ch);
    }
    std::cerr << "\nexpected codepoints:";
    for (wchar_t ch : expected) {
        std::cerr << " " << static_cast<unsigned int>(ch);
    }
    std::cerr << "\n";
    std::exit(1);
}

void TestManifestParsesSchema() {
    const std::wstring json =
        LR"({"version":"1","packages":[{"id":"raw","name":"RAW 格式支持","version":"1.0.0","sha256":"abcdef","size":123,"url":"https://example.invalid/raw.zip","extensions":[".cr3",".nef"]}]})";
    const cpictures::CodecPackageManifest manifest = cpictures::ParseManifest(json);
    Expect(manifest.version == L"1", "manifest version");
    Expect(manifest.packages.size() == 1, "package count");
    Expect(manifest.packages[0].id == L"raw", "package id");
    Expect(manifest.packages[0].name == L"RAW 格式支持", "package name");
    Expect(manifest.packages[0].version == L"1.0.0", "package version");
    Expect(manifest.packages[0].sha256 == L"abcdef", "package sha256");
    Expect(manifest.packages[0].size == 123, "package size");
    Expect(manifest.packages[0].url == L"https://example.invalid/raw.zip", "package url");
    Expect(manifest.packages[0].extensions.size() == 2, "package extensions size");
    Expect(manifest.packages[0].extensions[1] == L".nef", "package extension");
}

void TestManifestMissingFieldsBecomeDefaults() {
    const cpictures::CodecPackageManifest manifest =
        cpictures::ParseManifest(LR"({"version":"2","packages":[{"id":"x"}]})");
    Expect(manifest.version == L"2", "manifest fallback version");
    Expect(manifest.packages.size() == 1, "manifest fallback package count");
    Expect(manifest.packages[0].id == L"x", "manifest fallback id");
    Expect(manifest.packages[0].name.empty(), "manifest fallback name");
    Expect(manifest.packages[0].size == 0, "manifest fallback size");
    Expect(manifest.packages[0].extensions.empty(), "manifest fallback extensions");
}

void TestManifestRejectsOversizedSizeSafely() {
    const cpictures::CodecPackageManifest manifest = cpictures::ParseManifest(
        LR"({"version":"1","packages":[{"id":"raw","size":18446744073709551616}]})");
    Expect(manifest.version.empty(), "oversized size returns empty manifest version");
    Expect(manifest.packages.empty(), "oversized size returns empty manifest packages");
}

void TestManifestParsesUnicodeEscapes() {
    const cpictures::CodecPackageManifest manifest = cpictures::ParseManifest(
        LR"({"version":"1","packages":[{"id":"raw","name":"RAW \u683C\u5F0F\u652F\u6301"}]})");
    Expect(manifest.packages.size() == 1, "unicode package count");
    ExpectWideEqual(manifest.packages[0].name,
                    L"RAW \x683C\x5F0F\x652F\x6301",
                    "unicode package name");
}

void TestSha256KnownValue() {
    const std::filesystem::path path =
        std::filesystem::temp_directory_path() / L"cpictures_sha256_abc.txt";
    {
        std::ofstream out(path, std::ios::binary);
        out << "abc";
    }

    const std::wstring hash = cpictures::ComputeSha256(path);
    Expect(hash == L"ba7816bf8f01cfea414140de5dae2223b00361a396177a9cb410ff61f20015ad",
           "sha256 known value");
    std::filesystem::remove(path);
}

void TestSha256MissingFileReturnsEmpty() {
    const std::filesystem::path path =
        std::filesystem::temp_directory_path() / L"cpictures_sha256_missing.txt";
    std::filesystem::remove(path);
    Expect(cpictures::ComputeSha256(path).empty(), "sha256 missing file");
}

}  // namespace

int main() {
    TestManifestParsesSchema();
    TestManifestMissingFieldsBecomeDefaults();
    TestManifestRejectsOversizedSizeSafely();
    TestManifestParsesUnicodeEscapes();
    TestSha256KnownValue();
    TestSha256MissingFileReturnsEmpty();
    std::cout << "update manifest tests passed\n";
    return 0;
}
