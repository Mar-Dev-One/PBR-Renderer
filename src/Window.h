#pragma once

#include "Core/Defines.h"

#include <GLFW/glfw3.h>

typedef struct window
{
    char* title;
    uint16 width;
    uint16 height;
    GLFWwindow *handle;
    GLFWwindow* parent_window;

} window;

b8 init_window(window* wind);
void show_window(window* window);
void destroy_window(window* window);
