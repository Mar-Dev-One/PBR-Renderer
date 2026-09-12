#include "RHI.h"
#include "RHI_Backend.h"

#include "OpenGL/RHI_GL.h"
// Add new backend headers here as new src/RHI/<Api>/ folders appear,
// e.g. #include "Vulkan/RHI_VK.h"

static rhi_backend_api backend;

b8 rhi_init(rhi_backend_type type, rhi_proc_loader loader)
{
    switch (type)
    {
        case RHI_BACKEND_OPENGL:
            rhi_gl_get_backend(&backend);
            break;

        default:
            FATAL("Unknown or unbuilt RHI backend");
            return false;
    }

    return backend.init(loader);
}

void rhi_shutdown(void)
{
    backend.shutdown();
}

void rhi_set_viewport(uint16 x, uint16 y, uint16 width, uint16 height)
{
    backend.set_viewport(x, y, width, height);
}

void rhi_clear(f32 r, f32 g, f32 b, f32 a)
{
    backend.clear(r, g, b, a);
}

rhi_buffer rhi_vertex_buffer_create(const void* data, uint64 size,
                                     const rhi_vertex_layout* layout,
                                     rhi_buffer_usage usage)
{
    return backend.vertex_buffer_create(data, size, layout, usage);
}

rhi_buffer rhi_index_buffer_create(const void* data, uint64 size, rhi_buffer_usage usage)
{
    return backend.index_buffer_create(data, size, usage);
}

void rhi_buffer_update(rhi_buffer buffer, const void* data, uint64 size, uint64 offset)
{
    backend.buffer_update(buffer, data, size, offset);
}

void rhi_buffer_destroy(rhi_buffer buffer)
{
    backend.buffer_destroy(buffer);
}

rhi_shader rhi_shader_create(rhi_shader_desc desc)
{
    return backend.shader_create(desc);
}

void rhi_shader_bind(rhi_shader shader)
{
    backend.shader_bind(shader);
}

void rhi_shader_destroy(rhi_shader shader)
{
    backend.shader_destroy(shader);
}

void rhi_texture_bind(rhi_texture texture, uint32 slot)
{
    backend.texture_bind(texture, slot);
}

rhi_texture rhi_texture_create(rhi_texture_desc desc)
{
    return backend.texture_create(desc);
}

void rhi_texture_destroy(rhi_texture texture)
{
    backend.texture_destroy(texture);
}

rhi_framebuffer rhi_framebuffer_create(rhi_framebuffer_desc desc)
{
    ASSERT(desc.color_attachment_count <= RHI_MAX_COLOR_ATTACHMENTS);
    return backend.framebuffer_create(desc);
}

void rhi_framebuffer_bind(rhi_framebuffer framebuffer)
{
    backend.framebuffer_bind(framebuffer);
}

void rhi_framebuffer_bind_default(void)
{
    backend.framebuffer_bind_default();
}

rhi_texture rhi_framebuffer_get_color_texture(rhi_framebuffer framebuffer, uint32 index)
{
    return backend.framebuffer_get_color_texture(framebuffer, index);
}

rhi_texture rhi_framebuffer_get_depth_texture(rhi_framebuffer framebuffer)
{
    return backend.framebuffer_get_depth_texture(framebuffer);
}

void rhi_framebuffer_destroy(rhi_framebuffer framebuffer)
{
    backend.framebuffer_destroy(framebuffer);
}

void rhi_draw_indexed(rhi_buffer vertex_buffer, rhi_buffer index_buffer, uint32 index_count)
{
    backend.draw_indexed(vertex_buffer, index_buffer, index_count);
}


////
void rhi_read_pixels(uint16 x, uint16 y, uint16 width, uint16 height, void* out_pixels)
{
    backend.read_pixels(x, y, width, height, out_pixels);
}