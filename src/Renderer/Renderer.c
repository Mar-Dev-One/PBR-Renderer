#include "Renderer.h"

#include "../RHI/RHI.h"

static renderer main_renderer;

renderer get_renderer()
{
    return main_renderer;
}

b8 init_renderer(window_descriptor init_window_desc)
{
    if (!main_renderer)
        main_renderer = malloc(sizeof(*main_renderer));

    main_renderer->drawing_window = window_create(init_window_desc);

    // Only OpenGL is implemented right now — this is the one place
    // that picks a backend; everything above Renderer never sees it.
    if (!rhi_init(RHI_BACKEND_OPENGL, window_get_gl_loader()))
        FATAL("Failed to initialize RHI");

    return true;
}

void renderer_begin_frame()
{
    window_poll_events();
}

void renderer_end_frame()
{
    window_swap_buffers(main_renderer->drawing_window);
}

void renderer_set_viewport(uint16 x, uint16 y, uint16 width, uint16 height)
{
    rhi_set_viewport(x, y, width, height);
}

void renderer_clear(f32 r, f32 g, f32 b, f32 a)
{
    rhi_clear(r, g, b, a);
}

void terminate_renderer()
{
    rhi_shutdown();
    window_destroy(main_renderer->drawing_window);
    free(main_renderer);
}
