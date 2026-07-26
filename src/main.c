#include <stdio.h>

#include "Renderer.h"
#include "Window.h"

int main(void)
{
    renderer opengl_renderer;
    renderer_init(&opengl_renderer);

    window main_window = {
        .title = "Main window",
        .height = 800,
        .width = 600,
        .parent_window = NULL,
        .handle = NULL
    };

    init_window(&main_window);

    set_target_window(&opengl_renderer, &main_window);

    renderer_on_update(&opengl_renderer, 0.0f);

    renderer_destroy(&opengl_renderer);
    destroy_window(&main_window);


    

    return 0;
}