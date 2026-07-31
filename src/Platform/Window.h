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
typedef void(* close_callback) (GLFWwindow *window);


window* window_create(window_descriptor desc);


void window_poll_events(void);
void window_swap_buffers(window* wind);
void window_make_context_current(window* wind);
b8   window_should_close(window* wind);
void window_close(window* wind);

void window_set_width(window* wind, uint16 width);
void window_set_height(window* wind, uint16 height);


void window_set_key_callback(window* wind, key_callback callback);
void window_set_resize_callback(window* wind, resize_callback callback);
void window_set_close_callback(window* wind, close_callback callback);


window_size window_get_size(window* wind);
void window_set_title(window* wind, const char* title);


void window_destroy(window* wind);