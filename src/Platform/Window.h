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


window* window_create(window_descriptor desc);


void window_poll_events(void);
void window_swap_buffers(window* wind);
b8   window_should_close(window* wind);
void window_close(window* wind);

window_size window_get_size(window* wind);
void window_set_title(window* wind, const char* title);


void window_destroy(window* wind);