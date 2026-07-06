#include <stddef.h>
#include <stdint.h>

#include "codecs/cpictures_codec.h"

static int CPICTURES_CALLCONV StubInfo(cpictures_codec_info* out_info) {
    (void)out_info;
    return 0;
}

static int CPICTURES_CALLCONV StubProbe(const wchar_t* path) {
    (void)path;
    return 0;
}

static int CPICTURES_CALLCONV StubDecode(const wchar_t* path, cpictures_decoded_image* out_image) {
    (void)path;
    (void)out_image;
    return 0;
}

static void CPICTURES_CALLCONV StubFree(cpictures_decoded_image* image) {
    (void)image;
}

int main(void) {
    cpictures_codec_info info = {0};
    cpictures_decoded_image image = {0};
    cpictures_codec_info_fn info_fn = &StubInfo;
    cpictures_probe_fn probe_fn = &StubProbe;
    cpictures_decode_fn decode_fn = &StubDecode;
    cpictures_free_image_fn free_fn = &StubFree;

    (void)info;
    (void)image;
    (void)info_fn;
    (void)probe_fn;
    (void)decode_fn;
    (void)free_fn;
    return 0;
}
