#include "Renderer.h"

#include "glfw/glfw3.h"


static renderer main_renderer;

renderer get_renderer()
{
    return main_renderer;
}

b8 init_renderer(window_descriptor init_window_desc)
{
    if (!main_renderer)
        main_renderer = malloc(sizeof(renderer));

    main_renderer->drawing_window = window_create(init_window_desc);
    
    if (!gladLoadGL((GLADloadfunc)glfwGetProcAddress))
        FATAL("Failed to initialize GLAD");

}

void renderer_begin_frame()
{

}

void renderer_end_frame()
{
    window_swap_buffers(main_renderer->drawing_window);
}

void renderer_set_viewport(uint16 x, uint16 y, uint16 width, uint16 height)
{
    glViewport(x, y ,width, height);
}

void renderer_clear(f32 r, f32 g, f32 b, f32 a)
{
    glClearColor(r, g, b ,a);
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
}


void terminate_renderer()
{
    window_destroy(main_renderer->drawing_window);
    free(main_renderer);
}