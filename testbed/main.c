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
#include "UI/ImGuiLayer.h"
#include "cimgui.h"

#include <math.h>
#include <GLFW/glfw3.h>

// Texture picker: every path/name pair the GUI combo box can switch
// between. Add an entry here (and drop the file in assets/textures/) to
// make a new texture selectable -- nothing else needs to change.
static const char* TEXTURE_PATHS[] = {
    "textures/checker.png",
    "textures/stripes.png",
    "textures/uv_grid.png",
};

static const char* TEXTURE_NAMES[] = {
    "Checkerboard",
    "Stripes",
    "UV Grid",
};

#define TEXTURE_COUNT (sizeof(TEXTURE_PATHS) / sizeof(TEXTURE_PATHS[0]))

typedef struct demo_state
{
    rhi_buffer  vertex_buffer;
    rhi_buffer  index_buffer;
    rhi_shader  shader;
    rhi_texture textures[TEXTURE_COUNT];
    int         selected_texture;   // index into textures[]/TEXTURE_NAMES, driven by the GUI combo box
    camera      cam;

    // Mouse-orbit camera control. The camera itself stays dumb (position +
    // target, per Camera.h) -- yaw/pitch/distance live here, in the layer
    // responsible for turning input into camera state, and position is
    // recomputed from them every frame.
    f32 orbit_yaw;
    f32 orbit_pitch;
    f32 orbit_distance;
    f64 last_cursor_x;
    f64 last_cursor_y;
    b8  dragging;
} demo_state;

// Cube: position (vec3) + uv (vec2) per vertex. Each face still gets its
// own 4 vertices (rather than sharing the 8 cube corners) so every face
// can have its own 0..1 UV range — sharing corners would smear a single
// UV attribute across faces that should each tile the texture the same
// way, since uv is per-vertex, not per-face.
static const f32 CUBE_VERTICES[] = {
    // Front face (+z)
    -0.5f, -0.5f,  0.5f,  0.0f, 0.0f,
     0.5f, -0.5f,  0.5f,  1.0f, 0.0f,
     0.5f,  0.5f,  0.5f,  1.0f, 1.0f,
    -0.5f,  0.5f,  0.5f,  0.0f, 1.0f,

    // Back face (-z)
    -0.5f, -0.5f, -0.5f,  0.0f, 0.0f,
     0.5f, -0.5f, -0.5f,  1.0f, 0.0f,
     0.5f,  0.5f, -0.5f,  1.0f, 1.0f,
    -0.5f,  0.5f, -0.5f,  0.0f, 1.0f,

    // Left face (-x)
    -0.5f, -0.5f, -0.5f,  0.0f, 0.0f,
    -0.5f, -0.5f,  0.5f,  1.0f, 0.0f,
    -0.5f,  0.5f,  0.5f,  1.0f, 1.0f,
    -0.5f,  0.5f, -0.5f,  0.0f, 1.0f,

    // Right face (+x)
     0.5f, -0.5f, -0.5f,  0.0f, 0.0f,
     0.5f, -0.5f,  0.5f,  1.0f, 0.0f,
     0.5f,  0.5f,  0.5f,  1.0f, 1.0f,
     0.5f,  0.5f, -0.5f,  0.0f, 1.0f,

    // Top face (+y)
    -0.5f,  0.5f,  0.5f,  0.0f, 0.0f,
     0.5f,  0.5f,  0.5f,  1.0f, 0.0f,
     0.5f,  0.5f, -0.5f,  1.0f, 1.0f,
    -0.5f,  0.5f, -0.5f,  0.0f, 1.0f,

    // Bottom face (-y)
    -0.5f, -0.5f,  0.5f,  0.0f, 0.0f,
     0.5f, -0.5f,  0.5f,  1.0f, 0.0f,
     0.5f, -0.5f, -0.5f,  1.0f, 1.0f,
    -0.5f, -0.5f, -0.5f,  0.0f, 1.0f,
};

static const uint32 CUBE_INDICES[] = {
     0,  1,  2,   2,  3,  0,   // front
     4,  6,  5,   6,  4,  7,   // back
     8,  9, 10,  10, 11,  8,   // left
    12, 14, 13,  14, 12, 15,   // right
    16, 17, 18,  18, 19, 16,   // top
    20, 22, 21,  22, 20, 23,   // bottom
};

static inline f32
clampf(f32 v, f32 lo, f32 hi)
{
    return v < lo ? lo : (v > hi ? hi : v);
}

// Recomputes cam->position from state's yaw/pitch/distance, orbiting around
// whatever cam->target currently is. Call after any yaw/pitch/distance
// change; cam->target itself is untouched so this only ever orbits, never
// pans.
static void
orbit_camera_apply(demo_state* state)
{
    f32 yaw_rad   = glm_rad(state->orbit_yaw);
    f32 pitch_rad = glm_rad(state->orbit_pitch);

    f32 x = state->orbit_distance * cosf(pitch_rad) * sinf(yaw_rad);
    f32 y = state->orbit_distance * sinf(pitch_rad);
    f32 z = state->orbit_distance * cosf(pitch_rad) * cosf(yaw_rad);

    state->cam.position[0] = state->cam.target[0] + x;
    state->cam.position[1] = state->cam.target[1] + y;
    state->cam.position[2] = state->cam.target[2] + z;
}

// Polls the left mouse button + cursor delta and turns it into orbit_yaw /
// orbit_pitch. Polling (rather than a glfwSetCursorPosCallback) keeps this
// self-contained in the demo layer. Goes through the window_* input
// wrappers rather than GLFW directly.
static void
orbit_camera_update(demo_state* state, window* wind)
{
    f64 cursor_x, cursor_y;
    window_get_cursor_pos(wind, &cursor_x, &cursor_y);

    b8 lmb_down        = window_is_mouse_button_down(wind, GLFW_MOUSE_BUTTON_LEFT);
    b8 imgui_has_mouse = imgui_layer_wants_mouse();

    if (lmb_down && !imgui_has_mouse)
    {
        if (state->dragging)
        {
            const f32 sensitivity = 0.25f;

            state->orbit_yaw   += (f32)(cursor_x - state->last_cursor_x) * sensitivity;
            state->orbit_pitch += (f32)(state->last_cursor_y - cursor_y) * sensitivity;
            state->orbit_pitch  = clampf(state->orbit_pitch, -89.0f, 89.0f);

            orbit_camera_apply(state);
        }

        state->dragging = true;
    }
    else
    {
        state->dragging = false;
    }

    state->last_cursor_x = cursor_x;
    state->last_cursor_y = cursor_y;
}

// Mouse-wheel zoom: shrinks/grows orbit_distance, clamped so you can't
// zoom through the cube or scroll off into the distance forever. App.c only
// calls this for scrolls ImGui didn't claim.
static void
on_scroll(App* app, f64 dx, f64 dy)
{
    (void)dx;

    demo_state* state = (demo_state*)app->user_data;

    const f32 zoom_speed = 0.5f;

    state->orbit_distance -= (f32)dy * zoom_speed;
    state->orbit_distance  = clampf(state->orbit_distance, 1.0f, 50.0f);

    orbit_camera_apply(state);
}

// Runs once from App.c's run(), after the GL context / RHI / ImGui exist.
static void
on_init(App* app)
{
    demo_state* state = (demo_state*)app->user_data;
    window_size framebuffer_size = window_get_size(get_renderer()->drawing_window);

    rhi_vertex_attribute attrs[] = {
        { .location = 0, .component_count = 3, .offset = 0 },
        { .location = 1, .component_count = 2, .offset = 3 * sizeof(f32) },
    };

    rhi_vertex_layout layout = {
        .attributes = attrs,
        .attribute_count = 2,
        .stride = 5 * sizeof(f32)
    };

    state->vertex_buffer = rhi_vertex_buffer_create(CUBE_VERTICES, sizeof(CUBE_VERTICES), &layout, RHI_USAGE_STATIC);
    state->index_buffer  = rhi_index_buffer_create(CUBE_INDICES, sizeof(CUBE_INDICES), RHI_USAGE_STATIC);

    char* vertex_path   = asset_path("shaders/textured.vert");
    char* fragment_path = asset_path("shaders/textured.frag");

    state->shader = rhi_shader_create_from_files(vertex_path, fragment_path);

    free(vertex_path);
    free(fragment_path);

    if (!state->shader)
        FATAL("demo: failed to load textured shader");

    for (uint32 i = 0; i < TEXTURE_COUNT; ++i)
    {
        char* texture_path = asset_path(TEXTURE_PATHS[i]);

        state->textures[i] = rhi_texture_create_from_file(texture_path,
                                                            RHI_FILTER_LINEAR,
                                                            RHI_WRAP_REPEAT,
                                                            /* generate_mipmaps */ true);

        free(texture_path);

        if (!state->textures[i])
            FATAL("demo: failed to load texture '%s'", TEXTURE_PATHS[i]);
    }

    state->selected_texture = 0;

    // Sampler binding is fixed at texture unit 0 for the lifetime of the
    // shader, so this only needs setting once here rather than every
    // frame — only the actual texture bound to unit 0 changes per-draw.
    rhi_shader_set_int(state->shader, "u_texture", 0);

    f32 aspect = (f32)framebuffer_size.width / (f32)framebuffer_size.height;

    vec3 initial_position = { 2.5f, 2.0f, 3.5f };
    vec3 initial_target   = { 0.0f, 0.0f, 0.0f };

    camera_init(&state->cam,
                initial_position,
                initial_target,               // target: look at the origin
                45.0f,                        // vertical FOV
                aspect,
                0.1f, 100.0f);

    // Derive yaw/pitch/distance from the position above so mouse-orbit
    // continues smoothly from wherever the camera already was, instead of
    // snapping to some default angle on the first drag.
    vec3 offset;
    glm_vec3_sub(initial_position, initial_target, offset);

    state->orbit_distance = glm_vec3_norm(offset);
    state->orbit_yaw       = glm_deg(atan2f(offset[0], offset[2]));
    state->orbit_pitch     = glm_deg(asinf(offset[1] / state->orbit_distance));
    state->dragging        = false;
}

// The GL viewport is already updated by App.c before this runs; the
// projection matrix needs the same treatment or the cube will stretch.
static void on_resize(App* app, uint16 width, uint16 height)
{
    demo_state* state = (demo_state*)app->user_data;

    // 0x0 while minimized -- would make the aspect NaN/inf.
    if (width == 0 || height == 0)
        return;

    camera_set_aspect(&state->cam, (f32)width / (f32)height);
}

static void on_key(App* app, int key, int action, int mods)
{
    (void)mods;

    if (key == GLFW_KEY_ESCAPE && action == GLFW_PRESS)
        app->should_close = true;
}

static void on_frame(App* app, f32 dt)
{
    (void)dt;

    demo_state* state = (demo_state*)app->user_data;
    window*     wind  = get_renderer()->drawing_window;

    // Left-drag to orbit the camera around the cube. Must run after
    // imgui_layer_new_frame() (WantCaptureMouse needs this frame's ImGui
    // state) which App.c already guarantees by calling it before on_frame.
    orbit_camera_update(state, wind);

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
    rhi_texture_bind(state->textures[state->selected_texture], 0);
    rhi_draw_indexed(state->vertex_buffer, state->index_buffer,
                      sizeof(CUBE_INDICES) / sizeof(CUBE_INDICES[0]));

    // App/on_frame runs between imgui_layer_new_frame() and
    // imgui_layer_render() (see App.c), so any ig*() calls here just work.
    // Fullscreen dockspace so ImGui windows (the texture picker included)
    // can actually be dragged and docked against something. dockspace_id=0
    // auto-generates an ID; NULL viewport = the main viewport; no flags,
    // no window-class restriction.
    igDockSpaceOverViewport(0, NULL, ImGuiDockNodeFlags_PassthruCentralNode, NULL);

    // Texture picker: a combo box bound directly to selected_texture, which
    // is what the draw call above reads every frame -- no extra plumbing
    // needed between "user picks an entry" and "the cube shows it".
    igBegin("Texture", NULL, 0);
    igCombo_Str_arr("Texture", &state->selected_texture, TEXTURE_NAMES, (int)TEXTURE_COUNT, -1);
    igEnd();
}

// Runs from terminate(), before the GL context goes away.
static void on_shutdown(App* app)
{
    demo_state* state = (demo_state*)app->user_data;

    for (uint32 i = 0; i < TEXTURE_COUNT; ++i)
        rhi_texture_destroy(state->textures[i]);

    rhi_shader_destroy(state->shader);
    rhi_buffer_destroy(state->index_buffer);
    rhi_buffer_destroy(state->vertex_buffer);
}

int main(void)
{
    // GPU resources (vertex/index buffers, shader) can only be created
    // after RHI is initialized, which happens inside run() -- on_init
    // creates them, on_shutdown destroys them.
    demo_state state = { 0 };

    App app = {
        .name = "PBR Renderer - Cube Demo",
        .on_init = on_init,
        .on_frame = on_frame,
        .on_shutdown = on_shutdown,
        .on_key = on_key,
        .on_scroll = on_scroll,
        .on_resize = on_resize,
        .user_data = &state,
        .should_close = false
    };

    run(&app);
    terminate(&app);

    return 0;
}
