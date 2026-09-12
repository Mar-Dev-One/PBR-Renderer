#pragma once

#include "Core/Defines.h"

struct App;

// Invoked once per frame from inside run(), after the frame is cleared and
// before it's presented. This is the extension point application code (and
// tests) use to actually draw something through the RHI/Renderer.
typedef void (*app_frame_callback)(struct App* app);

typedef struct App
{
    char* name;
    app_frame_callback on_frame; // optional, may be NULL
    void* user_data;             // optional, passed through to on_frame via app->user_data
    b8 should_close;             // set this from on_frame to exit run() without closing the window
} App;


void run(App* app);
void terminate(App* app);