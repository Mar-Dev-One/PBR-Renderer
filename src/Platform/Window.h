#pragma once

#include "../Core/Defines.h"

typedef struct window window;

typedef struct window_descriptor {
    const char* title;
    uint16      width;
    uint16      height;
    b8          resizable;
    b8          vsync;
} window_descriptor;

typedef struct window_size {
    uint16 width;
    uint16 height;
} window_size;


//TODO : Not expose the GLFW types like GLFWwindow
typedef struct GLFWwindow GLFWwindow;
typedef void (*key_callback)(GLFWwindow* window, int key, int scancode, int action, int mods);
typedef void (*resize_callback)(GLFWwindow* window, int width, int height);

// Generic GL proc-address loader, handed to the RHI backend so it can
// resolve GL functions without RHI having to include GLFW itself.
typedef void* (*gl_proc_loader)(const char* name);


window* window_create(window_descriptor desc);

gl_proc_loader window_get_gl_loader(void);

void window_poll_events(void);
void window_swap_buffers(window* wind);
b8   window_should_close(window* wind);
void window_close(window* wind);

void window_set_size(window* wind, uint16 width, uint16 height);
void window_set_title(window* wind, const char* title);

// Updates the window's cached width/height WITHOUT calling glfwSetWindowSize.
// Use this from a resize/framebuffer-size callback, where GLFW has already
// resized the window and you just need to record the new size — calling
// window_set_size() there would issue a redundant resize request back at
// GLFW from inside its own callback.
void window_on_resized(window* wind, uint16 width, uint16 height);

void window_set_key_callback(window* wind, key_callback callback);
void window_set_resize_callback(window* wind, resize_callback callback);

window_size window_get_size(window* wind);

void window_destroy(window* wind);