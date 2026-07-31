#include "Window.h"

#include "GLFW/glfw3.h"

#define WINDOW_TITLE_MAX_LEN 128

struct window {
    GLFWwindow* handle;
    char        title[WINDOW_TITLE_MAX_LEN];
    uint16      width;
    uint16      height;
    b8          vsync;
};

window* window_create(window_descriptor desc)
{
    if (!glfwInit())
        FATAL("GLFW has not been initialized properly");

    glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 4);
    glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 6);
    glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);
    glfwWindowHint(GLFW_RESIZABLE, desc.resizable ? GLFW_TRUE : GLFW_FALSE);

    GLFWwindow* handle = glfwCreateWindow(desc.width, desc.height, desc.title, NULL, NULL);

    if (!handle) {
        glfwTerminate();
        FATAL("Failed to create GLFW window");
    }

    glfwMakeContextCurrent(handle);
    glfwSwapInterval(desc.vsync ? 1 : 0);

    window* wind = malloc(sizeof(window));

    wind->handle = handle;
    wind->width = desc.width;
    wind->height = desc.height;
    wind->vsync = desc.vsync;
    strncpy(wind->title, desc.title, WINDOW_TITLE_MAX_LEN - 1);
    wind->title[WINDOW_TITLE_MAX_LEN - 1] = '\0';

    return wind;
}


void window_poll_events(void)
{
    glfwPollEvents();
}

void window_swap_buffers(window* wind)
{
    glfwSwapBuffers(wind->handle);
}

void window_make_context_current(window* wind)
{
    glfwMakeContextCurrent(wind->handle);
}

b8 window_should_close(window* wind)
{
    return glfwWindowShouldClose(wind->handle);
}

void window_close(window* wind)
{
    glfwSetWindowShouldClose(wind->handle, GLFW_TRUE);
}

window_size window_get_size(window* wind)
{
    window_size size = {
        .width = wind->width,
        .height = wind->height
    };

    return size;
}

void window_set_key_callback(window* wind, key_callback callback)
{
    glfwSetKeyCallback(wind->handle, callback);
}


void window_set_width(window* wind, uint16 width)
{
    wind->width = width;
}

void window_set_height(window* wind, uint16 height)
{
    wind->height = height;
}


void window_set_resize_callback(window* wind, resize_callback callback)
{
    glfwSetFramebufferSizeCallback(wind->handle, callback);
}

void window_set_close_callback(window* wind, close_callback callback)
{
    glfwSetWindowCloseCallback(wind->handle, callback);
}

void window_set_title(window* wind, const char* title)
{
    strncpy(wind->title, title, WINDOW_TITLE_MAX_LEN - 1);
    wind->title[WINDOW_TITLE_MAX_LEN - 1] = '\0';
}


void window_destroy(window* wind)
{
    glfwDestroyWindow(wind->handle);
    free(wind);
    LOG_INFO("Window destroyed!");

    //TODO : add GLFW Terminate; not now because we may have multiple windows
}