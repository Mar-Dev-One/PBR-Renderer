#include "Renderer.h"

#include "glad/gl.h"
#include "glfw/glfw3.h"

struct renderer {
    window** windows_array;
    window* drawing_window;
    uint8 windows_count;
};

static renderer main_renderer;

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
    glViewport(0, 0, width, height);
    window_set_width(main_renderer->drawing_window, width);
    window_set_height(main_renderer->drawing_window, height);    
}

static void on_close(GLFWwindow* window)
{
    glfwSetWindowShouldClose(window, false);
    glfwHideWindow(window);
    LOG_INFO("Closing window %p", window);
}

b8 init_renderer(window_descriptor* windows_desc, uint8 windows_number)
{
    if (!main_renderer)
        main_renderer = malloc(sizeof(renderer));

    main_renderer->windows_array = malloc(sizeof(window*) * windows_number);
    main_renderer->windows_count = windows_number;

    for (int i = 0; i < windows_number; ++i)
        main_renderer->windows_array[i] = window_create(windows_desc[i]);
        
    if (!gladLoadGL((GLADloadfunc)glfwGetProcAddress))
        FATAL("Failed to initialize GLAD");

    main_renderer->drawing_window = main_renderer->windows_array[0];

    
    for (int i = 0; i < windows_number; ++i)
    {
        window_set_key_callback(main_renderer->windows_array[i], keyboard_input_handler);
        window_set_resize_callback(main_renderer->windows_array[i], on_resize);
        window_set_close_callback(main_renderer->windows_array[i], on_close);
    }

}

void renderer_on_update()
{
    bool a = !window_should_close(main_renderer->windows_array[0]);

    for (int i = 1; i < main_renderer->windows_count; ++i)
            a = a || !window_should_close(main_renderer->windows_array[i]);

    while (a)
    {

        for (int i = 0; i < main_renderer->windows_count; ++i)
        {
            if (!window_should_close(main_renderer->windows_array[i]))
            {
                window_make_context_current(main_renderer->windows_array[i]);

                window_poll_events();

                glClearColor(0.1f, 0.1f, 0.12f, 1.0f);
                glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

                window_swap_buffers(main_renderer->windows_array[i]);
            }
        }

        for (int i = 1; i < main_renderer->windows_count; ++i)
            a = a || !window_should_close(main_renderer->windows_array[i]);

    }
}


void terminate_renderer()
{
    for (int i = 0; i < main_renderer->windows_count; ++i)
    {
        if (main_renderer->windows_array[i])
            window_destroy(main_renderer->windows_array[i]);
    }

}