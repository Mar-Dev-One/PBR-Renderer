#include "Image.h"

#define STB_IMAGE_IMPLEMENTATION
// stb_image logs its own errors via stbi_failure_reason(); we don't need
// the STBI_FAILURE_USERMSG variant, the default reason strings are enough.
#include "stb_image.h"

image image_load(const char* path, uint8 desired_channels)
{
    image img = { 0 };

    // The RHI/GL side of this codebase treats row 0 as the bottom of the
    // image (see rhi_read_pixels' doc comment and gl_texture_create's
    // upload), matching OpenGL's own (0,0)-at-bottom-left texture
    // convention. stb_image decodes top-down by default, so flip on load
    // once here rather than flipping UVs everywhere a texture is sampled.
    stbi_set_flip_vertically_on_load(true);

    int width, height, source_channels;
    stbi_uc* pixels = stbi_load(path, &width, &height, &source_channels,
                                 (int)desired_channels);

    if (!pixels)
    {
        LOG_ERROR("Could not load image '%s': %s\n", path, stbi_failure_reason());
        return img;
    }

    img.pixels = pixels;
    img.width = (uint16)width;
    img.height = (uint16)height;
    img.channel_count = desired_channels ? desired_channels : (uint8)source_channels;

    return img;
}

void image_free(image* img)
{
    if (!img || !img->pixels)
        return;

    stbi_image_free(img->pixels);
    img->pixels = NULL;
}
