#pragma once

#include "../Core/Defines.h"

// Opaque GPU resource handles. Concrete layout lives in the backend
// (e.g. src/RHI/OpenGL/) — nothing outside a backend's own files should
// ever know what's inside these.
typedef struct rhi_buffer*      rhi_buffer;
typedef struct rhi_shader*      rhi_shader;
typedef struct rhi_texture*     rhi_texture;
typedef struct rhi_framebuffer* rhi_framebuffer;

// Generic GL-style proc-address loader, supplied by the Platform layer
// (see window_get_gl_loader in Platform/Window.h) so RHI never has to
// include GLFW directly.
typedef void* (*rhi_proc_loader)(const char* name);

typedef enum rhi_buffer_usage
{
    RHI_USAGE_STATIC,   // data set once, drawn many times
    RHI_USAGE_DYNAMIC   // data updated frequently (rhi_buffer_update)
} rhi_buffer_usage;

typedef struct rhi_vertex_attribute
{
    uint32 location;          // matches `layout(location = N)` in the vertex shader
    uint32 component_count;   // e.g. 3 for a vec3
    uint32 offset;            // byte offset of this attribute within one vertex
} rhi_vertex_attribute;

typedef struct rhi_vertex_layout
{
    const rhi_vertex_attribute* attributes;
    uint32                      attribute_count;
    uint32                      stride;   // size in bytes of one vertex
} rhi_vertex_layout;

typedef struct rhi_shader_desc
{
    const char* vertex_src;
    const char* fragment_src;
} rhi_shader_desc;

#define RHI_MAX_COLOR_ATTACHMENTS 4

typedef enum rhi_texture_format
{
    RHI_FORMAT_RGB8,       // 8-bit per channel, no alpha — most loaded color/albedo images
    RHI_FORMAT_RGBA8,      // 8-bit per channel with alpha
    RHI_FORMAT_RGBA16F,    // HDR color (lighting accumulation, IBL, HDR framebuffers)
    RHI_FORMAT_DEPTH24     // depth attachment (shadow maps, depth pre-pass)
} rhi_texture_format;

typedef enum rhi_texture_filter
{
    RHI_FILTER_LINEAR,
    RHI_FILTER_NEAREST
} rhi_texture_filter;

typedef enum rhi_texture_wrap
{
    RHI_WRAP_REPEAT,
    RHI_WRAP_CLAMP_TO_EDGE
} rhi_texture_wrap;

typedef struct rhi_texture_desc
{
    uint16              width;
    uint16              height;
    rhi_texture_format  format;
    rhi_texture_filter  filter;            // magnification/minification filter
    rhi_texture_wrap    wrap;              // applied to both S and T
    const void*         pixels;            // pixel data matching `format`, or NULL for an empty/render-target texture
    b8                  generate_mipmaps;  // ignored if pixels is NULL
} rhi_texture_desc;

typedef struct rhi_framebuffer_attachment
{
    rhi_texture_format format;
} rhi_framebuffer_attachment;

typedef struct rhi_framebuffer_desc
{
    uint16                            width;
    uint16                            height;
    const rhi_framebuffer_attachment* color_attachments;    // NULL/0 for a depth-only target (e.g. shadow map)
    uint32                            color_attachment_count; // must be <= RHI_MAX_COLOR_ATTACHMENTS
    b8                                has_depth_attachment;
} rhi_framebuffer_desc;

// One folder per graphics API (src/RHI/OpenGL, src/RHI/Vulkan, ...).
// Only the backends actually compiled in are available; add to this
// enum as new backend folders are implemented.
typedef enum rhi_backend_type
{
    RHI_BACKEND_OPENGL
} rhi_backend_type;

// --- Device lifecycle --------------------------------------------------
// `loader` is only used to resolve GL-style function pointers; pass the
// value returned by window_get_gl_loader(). Must be called after the
// window's graphics context has been made current.
b8   rhi_init(rhi_backend_type backend, rhi_proc_loader loader);
void rhi_shutdown(void);

// --- Per-frame state -----------------------------------------------------
void rhi_set_viewport(uint16 x, uint16 y, uint16 width, uint16 height);
void rhi_clear(f32 r, f32 g, f32 b, f32 a);

// --- Buffers -------------------------------------------------------------
// Vertex buffers own their attribute layout at creation time (one-time
// setup cost instead of re-binding attributes on every draw call).
rhi_buffer rhi_vertex_buffer_create(const void* data, uint64 size,
                                     const rhi_vertex_layout* layout,
                                     rhi_buffer_usage usage);

rhi_buffer rhi_index_buffer_create(const void* data, uint64 size,
                                    rhi_buffer_usage usage);

////
void rhi_read_pixels(uint16 x, uint16 y, uint16 width, uint16 height, void* out_pixels);

// Sub-range update, e.g. for RHI_USAGE_DYNAMIC buffers.
void rhi_buffer_update(rhi_buffer buffer, const void* data, uint64 size, uint64 offset);
void rhi_buffer_destroy(rhi_buffer buffer);

// --- Shaders ---------------------------------------------------------
rhi_shader rhi_shader_create(rhi_shader_desc desc);

// Same as rhi_shader_create(), but reads the vertex/fragment source from
// disk first. Returns NULL if either file can't be read, or if compilation
// fails (see rhi_shader_create()).
rhi_shader rhi_shader_create_from_files(const char* vertex_path, const char* fragment_path);

void       rhi_shader_bind(rhi_shader shader);
void       rhi_shader_destroy(rhi_shader shader);

// --- Shader uniforms -----------------------------------------------------
// Uses GL's Direct State Access (glProgramUniform*, core since GL 4.1) —
// the shader does NOT need to be bound first. `matrix` is 16 floats,
// column-major (the layout cglm's mat4 already uses, so callers can just
// pass `(const f32*)&some_mat4[0][0]`).
void rhi_shader_set_mat4(rhi_shader shader, const char* name, const f32* matrix);
void rhi_shader_set_vec3(rhi_shader shader, const char* name, f32 x, f32 y, f32 z);
void rhi_shader_set_int(rhi_shader shader, const char* name, int32 value);

// --- Textures ----------------------------------------------------------
// Standalone textures — loaded image data (albedo/normal/roughness maps,
// once something in the codebase decodes image files) as well as empty
// render-target textures. Framebuffer attachments are created the same
// way internally; rhi_framebuffer_get_color_texture just hands one back.
rhi_texture rhi_texture_create(rhi_texture_desc desc);
void        rhi_texture_bind(rhi_texture texture, uint32 slot);
void        rhi_texture_destroy(rhi_texture texture);

// --- Framebuffers --------------------------------------------------------
// Render targets for offscreen passes: shadow maps, HDR scene color,
// G-buffer, IBL convolution, etc. Rendering to the screen doesn't need
// one — use rhi_framebuffer_bind_default() to point draws back at it.
rhi_framebuffer rhi_framebuffer_create(rhi_framebuffer_desc desc);
void            rhi_framebuffer_bind(rhi_framebuffer framebuffer);
void            rhi_framebuffer_bind_default(void);
rhi_texture     rhi_framebuffer_get_color_texture(rhi_framebuffer framebuffer, uint32 index);
rhi_texture     rhi_framebuffer_get_depth_texture(rhi_framebuffer framebuffer);
void            rhi_framebuffer_destroy(rhi_framebuffer framebuffer);

// --- Drawing ------------------------------------------------------------
void rhi_draw_indexed(rhi_buffer vertex_buffer, rhi_buffer index_buffer, uint32 index_count);


