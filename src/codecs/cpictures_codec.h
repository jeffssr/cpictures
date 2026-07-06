#pragma once

#include <stddef.h>
#include <stdint.h>
#include <wchar.h>

#ifdef _WIN32
#define CPICTURES_CODEC_EXPORT __declspec(dllexport)
#define CPICTURES_CALLCONV __stdcall
#else
#define CPICTURES_CODEC_EXPORT
#define CPICTURES_CALLCONV
#endif

typedef struct cpictures_codec_info {
    uint32_t abi_version;
    const wchar_t* name;
    const wchar_t* version;
    const wchar_t* extensions;
} cpictures_codec_info;

typedef struct cpictures_decoded_image {
    uint32_t width;
    uint32_t height;
    uint32_t stride;
    uint8_t* bgra;
    uint64_t bgra_size;
} cpictures_decoded_image;

#ifdef __cplusplus
extern "C" {
#endif

typedef int(CPICTURES_CALLCONV* cpictures_codec_info_fn)(cpictures_codec_info* out_info);
typedef int(CPICTURES_CALLCONV* cpictures_probe_fn)(const wchar_t* path);
typedef int(CPICTURES_CALLCONV* cpictures_decode_fn)(const wchar_t* path, cpictures_decoded_image* out_image);
typedef void(CPICTURES_CALLCONV* cpictures_free_image_fn)(cpictures_decoded_image* image);

#ifdef __cplusplus
}
#endif
