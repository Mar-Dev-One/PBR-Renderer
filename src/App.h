#pragma once

#include "Core/Defines.h"

#include "OpenGL/Renderer.h"

typedef struct App
{
    char* name;
} App;

static void keyboard_input_handler(GLFWwindow* window,
                  int key,
                  int scancode,
                  int action,
                  int mods)
{
    printf("Key %d Action %d\n", key, action);
}

static void on_resize(GLFWwindow* window, int width, int height)
{
    renderer rend = get_renderer();
    renderer_set_viewport(0, 0, width, height);
    window_set_size(rend->drawing_window, width, height);
}

void run(App* app)
{
    
    window_descriptor desc = {
        .title = app->name,
        .width = 800,
        .height = 600,
        .resizable = true,
        .vsync = true
    };
    
    init_renderer(desc);
    
    renderer rend = get_renderer();

    window_set_key_callback(rend->drawing_window, keyboard_input_handler);
    window_set_resize_callback(rend->drawing_window, on_resize);

    while (!window_should_close(rend->drawing_window)) {
        renderer_begin_frame();

        window_poll_events();

        renderer_clear(0.4f, 0.1f, 0.12f, 1.0f);

        
        LOG_INFO("width : %d, height %d\n", 
            window_get_size(rend->drawing_window).width,
            window_get_size(rend->drawing_window).height
        );
        
        renderer_end_frame();
    }
}

void terminate(App* app)
{
    terminate_renderer();
}