#include "Renderer.h"

#include "glad/gl.h"
#include "glfw/glfw3.h"

struct renderer {
    window* drawing_window;
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

b8 init_renderer(window_descriptor init_window_desc)
{
    if (!main_renderer)
        main_renderer = malloc(sizeof(renderer));

    main_renderer->drawing_window = window_create(init_window_desc);
    
    if (!gladLoadGL((GLADloadfunc)glfwGetProcAddress))
        FATAL("Failed to initialize GLAD");

    window_set_key_callback(main_renderer->drawing_window, keyboard_input_handler);
    window_set_resize_callback(main_renderer->drawing_window, on_resize);

}

void renderer_on_update()
{
    while (!window_should_close(main_renderer->drawing_window)) {
        window_poll_events();

        glClearColor(0.1f, 0.1f, 0.12f, 1.0f);
        glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

        window_swap_buffers(main_renderer->drawing_window);

        LOG_INFO("width : %d, height %d\n", 
            window_get_size(main_renderer->drawing_window).width,
            window_get_size(main_renderer->drawing_window).height
        );
    }
}


void terminate_renderer()
{
    window_destroy(main_renderer->drawing_window);
    free(main_renderer);
}