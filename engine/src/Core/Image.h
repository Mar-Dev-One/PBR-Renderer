#pragma once

#include "Defines.h"

// Decoded image data, always top-left origin as loaded from disk (see
// image_load's flip note). channel_count is whatever the source file
// actually has (1 = grey, 3 = RGB, 4 = RGBA) unless a specific channel
// count is requested — see image_load.
typedef struct image
{
    uint8* pixels;   // channel_count bytes per pixel, row-major, no padding
    uint16 width;
    uint16 height;
    uint8  channel_count;
} image;

// Loads and decodes an image file (PNG, JPG, BMP, TGA, ... — anything
// stb_image supports) into 8-bit-per-channel pixel data.
//
// `desired_channels` forces the decoded output to that many channels
// (e.g. pass 4 to always get RGBA regardless of the source file), or
// pass 0 to keep whatever the file itself has.
//
// Returns an image with pixels == NULL on failure (missing file, unknown
// format, decode error) and logs why. Caller owns image.pixels on success
// and must free it with image_free().
image image_load(const char* path, uint8 desired_channels);

// Same as image_load(), but decodes an image that's already in memory
// (an embedded glTF/GLB texture, a network download, ...). `data` is the
// still-encoded file contents (PNG/JPG bytes), not raw pixels. Same flip,
// channel and failure behaviour as image_load().
image image_load_from_memory(const uint8* data, uint64 size, uint8 desired_channels);

void  image_free(image* img);
