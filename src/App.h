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
    glViewport(0, 0, width, height);
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

    renderer_on_update();
}

void terminate(App* app)
{
    terminate_renderer();
}