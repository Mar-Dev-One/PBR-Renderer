#include "Renderer.h"



b8 renderer_init(renderer* renderer)
{
    int version = gladLoadGL(glfwGetProcAddress);
    if (version == 0)
    {
        fprintf(stderr, "Failed to initialize GLAD\n");
        
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

b8 set_target_window(renderer* renderer, window* window)
{
    renderer->current_window = window;
    glfwMakeContextCurrent(renderer->current_window->handle);

    return true;
}

b8 renderer_on_update(renderer* renderer, f32 delta_time)
{
    while (!glfwWindowShouldClose(renderer->current_window->handle))
    {
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
