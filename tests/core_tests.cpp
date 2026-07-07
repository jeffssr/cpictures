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
    Expect(text == L"17/128  DSC_1042.NEF", "overlay text puts index before file name");
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

    const cpictures::SizeI fitSmall = cpictures::ScaleImageToFitWorkArea(small, work);
    Expect(fitSmall.width == 1440, "fit-to-screen upscales small image width");
    Expect(fitSmall.height == 1080, "fit-to-screen upscales small image height");

    const cpictures::SizeI zoomedIn = cpictures::ScaleImageByZoomToWorkArea(small, work, 1.25);
    Expect(zoomedIn.width == 800, "zoom in expands window width");
    Expect(zoomedIn.height == 600, "zoom in expands window height");

    const cpictures::SizeI zoomedOut = cpictures::ScaleImageByZoomToWorkArea(small, work, 0.5);
    Expect(zoomedOut.width == 320, "zoom out shrinks window width");
    Expect(zoomedOut.height == 240, "zoom out shrinks window height");

    const cpictures::SizeI hugeZoom = cpictures::ScaleImageByZoomToWorkArea(image, work, 2.0);
    Expect(hugeZoom.width == 1440, "zoomed large image remains bounded width");
    Expect(hugeZoom.height == 1080, "zoomed large image remains bounded height");

    const cpictures::SizeI tall{800, 4000};
    const cpictures::SizeI tallViewport = cpictures::ActualSizeViewport(tall, work);
    Expect(tallViewport.width == 800, "tall image keeps 100 percent width");
    Expect(tallViewport.height == 1080, "tall image caps viewport height");

    const cpictures::SizeI wide{4000, 800};
    const cpictures::SizeI wideViewport = cpictures::ActualSizeViewport(wide, work);
    Expect(wideViewport.width == 1920, "wide image caps viewport width");
    Expect(wideViewport.height == 800, "wide image keeps 100 percent height");

    const cpictures::SizeI hugeViewport = cpictures::ActualSizeViewport(image, work);
    Expect(hugeViewport.width == 1920, "huge image caps viewport width");
    Expect(hugeViewport.height == 1080, "huge image caps viewport height");

    const cpictures::PointI clamped = cpictures::ClampPanOffset({900, -20}, {4000, 3000}, work);
    Expect(clamped.x == 900, "pan x inside range");
    Expect(clamped.y == 0, "pan y clamps to zero");

    const cpictures::PointI maxPan = cpictures::ClampPanOffset({9999, 9999}, {4000, 3000}, work);
    Expect(maxPan.x == 2080, "pan x clamps to max");
    Expect(maxPan.y == 1920, "pan y clamps to max");
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
