#include <cstdlib>
#include <iostream>
#include <algorithm>
#include <filesystem>
#include <fstream>
#include <string>
#include <vector>

#include "cpictures/geometry.h"
#include "cpictures/natural_sort.h"
#include "cpictures/overlay.h"
#include "cpictures/supported_formats.h"
#include "cpictures/image_list.h"

namespace {
void Expect(bool condition, const char* message) {
    if (!condition) {
        std::cerr << "FAIL: " << message << "\n";
        std::exit(1);
    }
}

void TestNaturalSort() {
    std::vector<std::wstring> names{L"10.jpg", L"2.jpg", L"1.jpg"};
    std::sort(names.begin(), names.end(), cpictures::NaturalLess);
    Expect(names[0] == L"1.jpg", "natural sort first item");
    Expect(names[1] == L"2.jpg", "natural sort second item");
    Expect(names[2] == L"10.jpg", "natural sort third item");
}

void TestOverlayText() {
    const std::wstring text = cpictures::BuildOverlayText(L"DSC_1042.NEF", 16, 128);
    Expect(text == L"DSC_1042.NEF  17/128", "overlay text uses one-based index");
}

void TestSupportedFormats() {
    Expect(cpictures::IsCoreSupportedExtension(L".jpg"), "jpg supported");
    Expect(cpictures::IsCoreSupportedExtension(L".JPEG"), "jpeg case insensitive");
    Expect(!cpictures::IsCoreSupportedExtension(L".psd"), "psd not core supported");
}

void TestFitWindow() {
    const cpictures::SizeI image{4000, 3000};
    const cpictures::SizeI work{1920, 1080};
    const cpictures::SizeI fitted = cpictures::FitImageWindow(image, work);
    Expect(fitted.width == 1440, "fit width keeps aspect ratio");
    Expect(fitted.height == 1080, "fit height reaches work area");

    const cpictures::SizeI small{640, 480};
    const cpictures::SizeI exact = cpictures::FitImageWindow(small, work);
    Expect(exact.width == 640, "small image remains 100 percent width");
    Expect(exact.height == 480, "small image remains 100 percent height");
}

void WriteTinyFile(const std::filesystem::path& path) {
    std::ofstream out(path, std::ios::binary);
    out << "x";
}

void TestImageListNaturalOrder() {
    const auto dir = std::filesystem::temp_directory_path() / L"cpictures_list_test";
    std::filesystem::remove_all(dir);
    std::filesystem::create_directories(dir);
    WriteTinyFile(dir / L"10.jpg");
    WriteTinyFile(dir / L"2.jpg");
    WriteTinyFile(dir / L"1.jpg");
    WriteTinyFile(dir / L"notes.txt");

    cpictures::ImageList list = cpictures::ImageList::LoadFromFile(dir / L"2.jpg");
    Expect(list.Count() == 3, "image list counts supported files");
    Expect(list.Index() == 1, "image list locates current file");
    Expect(list.Current().filename() == L"2.jpg", "current image is requested file");
    Expect(list.Next().filename() == L"10.jpg", "next image follows natural order");
    Expect(list.Previous().filename() == L"2.jpg", "previous returns to requested file");
    std::filesystem::remove_all(dir);
}

void TestImageListSkipsNonImageCurrentFile() {
    const auto dir = std::filesystem::temp_directory_path() / L"cpictures_list_skip_test";
    std::filesystem::remove_all(dir);
    std::filesystem::create_directories(dir);
    WriteTinyFile(dir / L"notes.txt");
    WriteTinyFile(dir / L"raw.ext");

    const cpictures::ImageList non_image_list = cpictures::ImageList::LoadFromFile(dir / L"notes.txt");
    Expect(non_image_list.Empty(), "existing non-image returns empty list");
    Expect(non_image_list.Count() == 0, "existing non-image count is zero");

    const cpictures::ImageList unsupported_list = cpictures::ImageList::LoadFromFile(dir / L"raw.ext");
    Expect(unsupported_list.Empty(), "existing unsupported extension returns empty list");
    Expect(unsupported_list.Count() == 0, "existing unsupported extension count is zero");

    const cpictures::ImageList missing_jpg_list = cpictures::ImageList::LoadFromFile(dir / L"ghost.jpg");
    Expect(!missing_jpg_list.Empty(), "missing known image still returns list entry");
    Expect(missing_jpg_list.Count() == 1, "missing known image count is one");
    Expect(missing_jpg_list.Current().filename() == L"ghost.jpg", "missing known image keeps requested path");

    std::filesystem::remove_all(dir);
}
}

int main() {
    TestNaturalSort();
    TestOverlayText();
    TestSupportedFormats();
    TestFitWindow();
    TestImageListNaturalOrder();
    TestImageListSkipsNonImageCurrentFile();
    std::cout << "cpictures core tests passed\n";
    return 0;
}
