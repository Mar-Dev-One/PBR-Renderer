// demo.c
//
// Interactive feature demo: a rotating, perspective-projected cube. Exists
// alongside main.c (the automated RHI smoke test) to show the pieces that
// sit above raw RHI calls — Scene/Camera and the new RHI uniform-setting
// API — actually being used together: model/view/projection matrices built
// with cglm, uploaded to the shader every frame, driving real 3D geometry
// instead of a flat NDC triangle.

#include "App.h"
#include "Core/Defines.h"
#include "Core/Paths.h"
#include "RHI/RHI.h"
#include "Renderer/Renderer.h"
#include "Scene/Camera.h"
#include "Platform/Window.h"
#include "cimgui.h"

typedef struct demo_state
{
    b8         initialized;
    rhi_buffer vertex_buffer;
    rhi_buffer index_buffer;
    rhi_shader shader;
    camera     cam;
} demo_state;

// Cube: position (vec3) + color (vec3) per vertex. Each face gets its own
// 4 vertices (rather than sharing the 8 cube corners) so every face can
// have a distinct flat color — sharing corners would blend colors across
// faces since color is a per-vertex attribute, not a per-face one.
static const f32 CUBE_VERTICES[] = {
    // Front face (+z) - red
    -0.5f, -0.5f,  0.5f,  1.0f, 0.0f, 0.0f,
     0.5f, -0.5f,  0.5f,  1.0f, 0.0f, 0.0f,
     0.5f,  0.5f,  0.5f,  1.0f, 0.0f, 0.0f,
    -0.5f,  0.5f,  0.5f,  1.0f, 0.0f, 0.0f,

    // Back face (-z) - green
    -0.5f, -0.5f, -0.5f,  0.0f, 1.0f, 0.0f,
     0.5f, -0.5f, -0.5f,  0.0f, 1.0f, 0.0f,
     0.5f,  0.5f, -0.5f,  0.0f, 1.0f, 0.0f,
    -0.5f,  0.5f, -0.5f,  0.0f, 1.0f, 0.0f,

    // Left face (-x) - blue
    -0.5f, -0.5f, -0.5f,  0.0f, 0.0f, 1.0f,
    -0.5f, -0.5f,  0.5f,  0.0f, 0.0f, 1.0f,
    -0.5f,  0.5f,  0.5f,  0.0f, 0.0f, 1.0f,
    -0.5f,  0.5f, -0.5f,  0.0f, 0.0f, 1.0f,

    // Right face (+x) - yellow
     0.5f, -0.5f, -0.5f,  1.0f, 1.0f, 0.0f,
     0.5f, -0.5f,  0.5f,  1.0f, 1.0f, 0.0f,
     0.5f,  0.5f,  0.5f,  1.0f, 1.0f, 0.0f,
     0.5f,  0.5f, -0.5f,  1.0f, 1.0f, 0.0f,

    // Top face (+y) - cyan
    -0.5f,  0.5f,  0.5f,  0.0f, 1.0f, 1.0f,
     0.5f,  0.5f,  0.5f,  0.0f, 1.0f, 1.0f,
     0.5f,  0.5f, -0.5f,  0.0f, 1.0f, 1.0f,
    -0.5f,  0.5f, -0.5f,  0.0f, 1.0f, 1.0f,

    // Bottom face (-y) - magenta
    -0.5f, -0.5f,  0.5f,  1.0f, 0.0f, 1.0f,
     0.5f, -0.5f,  0.5f,  1.0f, 0.0f, 1.0f,
     0.5f, -0.5f, -0.5f,  1.0f, 0.0f, 1.0f,
    -0.5f, -0.5f, -0.5f,  1.0f, 0.0f, 1.0f,
};

static const uint32 CUBE_INDICES[] = {
     0,  1,  2,   2,  3,  0,   // front
     4,  6,  5,   6,  4,  7,   // back
     8,  9, 10,  10, 11,  8,   // left
    12, 14, 13,  14, 12, 15,   // right
    16, 17, 18,  18, 19, 16,   // top
    20, 22, 21,  22, 20, 23,   // bottom
};

static void
demo_init(demo_state* state, window_size framebuffer_size)
{
    rhi_vertex_attribute attrs[] = {
        { .location = 0, .component_count = 3, .offset = 0 },
        { .location = 1, .component_count = 3, .offset = 3 * sizeof(f32) },
    };

    rhi_vertex_layout layout = {
        .attributes = attrs,
        .attribute_count = 2,
        .stride = 6 * sizeof(f32)
    };

    state->vertex_buffer = rhi_vertex_buffer_create(CUBE_VERTICES, sizeof(CUBE_VERTICES), &layout, RHI_USAGE_STATIC);
    state->index_buffer  = rhi_index_buffer_create(CUBE_INDICES, sizeof(CUBE_INDICES), RHI_USAGE_STATIC);

    char* vertex_path   = asset_path("shaders/unlit.vert");
    char* fragment_path = asset_path("shaders/unlit.frag");

    state->shader = rhi_shader_create_from_files(vertex_path, fragment_path);

    free(vertex_path);
    free(fragment_path);

    if (!state->shader)
        FATAL("demo: failed to load unlit shader");

    f32 aspect = (f32)framebuffer_size.width / (f32)framebuffer_size.height;

    camera_init(&state->cam,
                (vec3){ 2.5f, 2.0f, 3.5f },   // position
                (vec3){ 0.0f, 0.0f, 0.0f },   // target: look at the origin
                45.0f,                        // vertical FOV
                aspect,
                0.1f, 100.0f);

    state->initialized = true;
}

static void on_frame(App* app)
{
    demo_state* state = (demo_state*)app->user_data;
    window*     wind  = get_renderer()->drawing_window;

    if (!state->initialized)
        demo_init(state, window_get_size(wind));

    // Keep the camera's aspect ratio in sync with the window in case it was
    // resized since the last frame (on_resize already re-points the GL
    // viewport; the projection matrix needs the same treatment or the cube
    // will stretch).
    window_size size = window_get_size(wind);
    camera_set_aspect(&state->cam, (f32)size.width / (f32)size.height);

    f32 time = (f32)window_get_time();

    mat4 model;
    glm_mat4_identity(model);
    glm_rotate(model, time, (vec3){ 0.3f, 1.0f, 0.2f });

    mat4 view_projection;
    camera_get_view_projection(&state->cam, view_projection);

    mat4 mvp;
    glm_mat4_mul(view_projection, model, mvp);

    rhi_shader_set_mat4(state->shader, "u_mvp", (const f32*)mvp);

    rhi_shader_bind(state->shader);
    rhi_draw_indexed(state->vertex_buffer, state->index_buffer,
                      sizeof(CUBE_INDICES) / sizeof(CUBE_INDICES[0]));

    // App/on_frame runs between imgui_layer_new_frame() and
    // imgui_layer_render() (see App.c), so any ig*() calls here just work.
    // Demo window to prove the wiring is correct -- swap this out for real
    // panels once you're building your own UI.
    // Fullscreen dockspace so ImGui windows (the demo window included) can
    // actually be dragged and docked against something. dockspace_id=0
    // auto-generates an ID; NULL viewport = the main viewport; no flags,
    // no window-class restriction.
    igDockSpaceOverViewport(0, NULL, ImGuiDockNodeFlags_PassthruCentralNode, NULL);

    igShowDemoWindow(NULL);
}

int main(void)
{
    // GPU resources (vertex/index buffers, shader) can only be created
    // after RHI is initialized, which happens inside run() — so state
    // starts uninitialized and demo_init() runs lazily on the first frame.
    demo_state state = { .initialized = false };

    App app = {
        .name = "PBR Renderer - Cube Demo",
        .on_frame = on_frame,
        .user_data = &state,
        .should_close = false
    };

    run(&app);
    terminate(&app);

    return 0;
}
