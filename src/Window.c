#include "Window.h"

b8 init_window(window* wind)
{
    if (!glfwInit())
    {
        fprintf(stderr, "Failed to initialize GLFW\n");
        return -1;
    }

    glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 4);
    glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 6);
    glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);


    GLFWwindow *window = glfwCreateWindow(
        wind->width,
        wind->height,
        wind->title,
        NULL,
        wind->parent_window
    );

    if (!window)
    {
        fprintf(stderr, "Failed to create GLFW window\n");
        glfwTerminate();
        return false;
    }

    wind->handle = window;
    return true;
}

void show_window(window* window)
{
    glfwShowWindow(window->handle);
}

void destroy_window(window* window)
{
    glfwDestroyWindow(window->handle);
}