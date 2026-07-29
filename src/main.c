#include <stdio.h>

#include "OpenGL/Renderer.h"

int main(void)
{
    window_descriptor desc = {
        .title = "Hello OpenGL",
        .width = 800,
        .height = 600,
        .resizable = true,
        .vsync = false
    };

    init_renderer(desc);
    
    renderer_on_update();

    terminate_renderer();

    return 0;
}