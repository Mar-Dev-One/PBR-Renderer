#include "Renderer.h"

#include "glfw/glfw3.h"


static renderer main_renderer;

renderer get_renderer()
{
    return main_renderer;
}

b8 init_renderer(window_descriptor init_window_desc)
{
    if (!main_renderer)
        main_renderer = malloc(sizeof(renderer));

    main_renderer->drawing_window = window_create(init_window_desc);
    
    if (!gladLoadGL((GLADloadfunc)glfwGetProcAddress))
        FATAL("Failed to initialize GLAD");

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