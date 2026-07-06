#include <cstdlib>
#include <iostream>
#include <algorithm>
#include <string>
#include <vector>

#include "cpictures/geometry.h"
#include "cpictures/natural_sort.h"
#include "cpictures/overlay.h"
#include "cpictures/supported_formats.h"

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
}

int main() {
    TestNaturalSort();
    TestOverlayText();
    TestSupportedFormats();
    TestFitWindow();
    std::cout << "cpictures core tests passed\n";
    return 0;
}
