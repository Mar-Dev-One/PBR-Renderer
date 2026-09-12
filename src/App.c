#include "App.h"

#include "Renderer/Renderer.h"

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
    // GLFW already resized the window (this callback IS the notification) —
    // just record the new size and update the viewport. Calling
    // window_set_size() here would call glfwSetWindowSize() again from
    // inside GLFW's own resize callback.
    window_on_resized(rend->drawing_window, (uint16)width, (uint16)height);
    renderer_set_viewport(0, 0, (uint16)width, (uint16)height);
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

    while (!window_should_close(rend->drawing_window) && !app->should_close) {
        renderer_begin_frame();

        renderer_clear(0.4f, 0.1f, 0.12f, 1.0f);

        if (app->on_frame)
            app->on_frame(app);

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