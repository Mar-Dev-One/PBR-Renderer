#include "Renderer.h"



b8 renderer_init(renderer* renderer)
{
    renderer->current_window = &renderer->current_window_storage;
    renderer->current_window->title = "Main window";
    renderer->current_window->width = 800;
    renderer->current_window->height = 600;
    renderer->current_window->parent_window = NULL;
    renderer->current_window->handle = NULL;

    if (!init_window(renderer->current_window))
    {
        LOG_ERROR("Failed to initialize window");
        return false;
    }

    if (!_set_target_window(renderer, renderer->current_window))
    {
        LOG_ERROR("Failed to set target window");
        return false;
    }

    int version = gladLoadGL(glfwGetProcAddress);
    if (version == 0)
    {
        LOG_ERROR("Failed to load OpenGL functions");
        return false;
    }

    printf("Loaded OpenGL %d.%d\n",
           GLAD_VERSION_MAJOR(version),
           GLAD_VERSION_MINOR(version));

    printf("Vendor   : %s\n", glGetString(GL_VENDOR));
    printf("Renderer : %s\n", glGetString(GL_RENDERER));
    printf("Version  : %s\n", glGetString(GL_VERSION));

    return true;
}

b8 _set_target_window(renderer* renderer, window* window)
{
    if (!renderer || !window || !window->handle)
    {
        return false;
    }

    renderer->current_window = window;
    glfwMakeContextCurrent(renderer->current_window->handle);

    return true;
}

void renderer_on_update(renderer* renderer, f32 delta_time)
{
    f64 last_time = glfwGetTime();

    show_window(renderer->current_window);

    while (!glfwWindowShouldClose(renderer->current_window->handle))
    {
        f64 current_time = glfwGetTime();
        delta_time = (f32)(current_time - last_time);
        last_time = current_time;

        int width, height;
        glfwGetFramebufferSize(renderer->current_window->handle, &width, &height);

        glViewport(0, 0, width, height);

        glClearColor(0.1f, 0.2f, 0.3f, 1.0f);
        glClear(GL_COLOR_BUFFER_BIT);

        glfwSwapBuffers(renderer->current_window->handle);
        glfwPollEvents();
    }
}

void renderer_destroy(renderer* renderer)
{
}