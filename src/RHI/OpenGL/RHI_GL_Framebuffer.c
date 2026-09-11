#include "RHI_GL_Internal.h"

static rhi_texture
create_attachment(rhi_texture_format format, uint16 width, uint16 height)
{
    rhi_texture_desc desc = {
        .width = width,
        .height = height,
        .format = format,
        .filter = RHI_FILTER_LINEAR,
        .wrap = RHI_WRAP_CLAMP_TO_EDGE,
        .pixels = NULL,           // render targets start empty — the GPU fills them in
        .generate_mipmaps = false
    };

    return gl_texture_create(desc);
}

rhi_framebuffer gl_framebuffer_create(rhi_framebuffer_desc desc)
{
    ASSERT(desc.color_attachment_count <= RHI_MAX_COLOR_ATTACHMENTS);

    rhi_framebuffer fb = malloc(sizeof(*fb));
    if (!fb)
        FATAL("Out of memory creating framebuffer");

    fb->width = desc.width;
    fb->height = desc.height;
    fb->color_texture_count = desc.color_attachment_count;
    fb->depth_texture = NULL;

    glGenFramebuffers(1, &fb->fbo);
    glBindFramebuffer(GL_FRAMEBUFFER, fb->fbo);

    GLenum draw_buffers[RHI_MAX_COLOR_ATTACHMENTS];

    for (uint32 i = 0; i < desc.color_attachment_count; ++i)
    {
        rhi_texture tex = create_attachment(desc.color_attachments[i].format, desc.width, desc.height);
        fb->color_textures[i] = tex;

        glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0 + i,
                                GL_TEXTURE_2D, tex->handle, 0);
        draw_buffers[i] = GL_COLOR_ATTACHMENT0 + i;
    }

    if (desc.color_attachment_count > 0)
        glDrawBuffers((GLsizei)desc.color_attachment_count, draw_buffers);
    else
        glDrawBuffer(GL_NONE);   // depth-only target, e.g. a shadow map

    if (desc.has_depth_attachment)
    {
        fb->depth_texture = create_attachment(RHI_FORMAT_DEPTH24, desc.width, desc.height);
        glFramebufferTexture2D(GL_FRAMEBUFFER, GL_DEPTH_ATTACHMENT,
                                GL_TEXTURE_2D, fb->depth_texture->handle, 0);
    }

    if (glCheckFramebufferStatus(GL_FRAMEBUFFER) != GL_FRAMEBUFFER_COMPLETE)
        FATAL("Framebuffer incomplete");

    glBindFramebuffer(GL_FRAMEBUFFER, 0);

    return fb;
}

void gl_framebuffer_bind(rhi_framebuffer framebuffer)
{
    glBindFramebuffer(GL_FRAMEBUFFER, framebuffer->fbo);
    glViewport(0, 0, framebuffer->width, framebuffer->height);
}

void gl_framebuffer_bind_default(void)
{
    glBindFramebuffer(GL_FRAMEBUFFER, 0);
}

rhi_texture gl_framebuffer_get_color_texture(rhi_framebuffer framebuffer, uint32 index)
{
    ASSERT(index < framebuffer->color_texture_count);
    return framebuffer->color_textures[index];
}

rhi_texture gl_framebuffer_get_depth_texture(rhi_framebuffer framebuffer)
{
    return framebuffer->depth_texture;
}

void gl_framebuffer_destroy(rhi_framebuffer framebuffer)
{
    if (!framebuffer)
        return;

    for (uint32 i = 0; i < framebuffer->color_texture_count; ++i)
        gl_texture_destroy(framebuffer->color_textures[i]);

    gl_texture_destroy(framebuffer->depth_texture);

    glDeleteFramebuffers(1, &framebuffer->fbo);
    free(framebuffer);
}
