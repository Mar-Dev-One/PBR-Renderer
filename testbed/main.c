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
#include "Scene/Model.h"
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

// Which of the two demo scenes on_frame draws this frame -- both share the
// same camera/orbit controls, only the geometry/shader/GUI panel differ.
typedef enum render_mode
{
    RENDER_MODE_TEXTURED_CUBE,
    RENDER_MODE_LIT_MODEL
} render_mode;

typedef struct demo_state
{
    render_mode mode;
    rhi_buffer  vertex_buffer;
    rhi_buffer  index_buffer;
    rhi_shader  shader;
    rhi_texture textures[TEXTURE_COUNT];
    int         selected_texture;   // index into textures[]/TEXTURE_NAMES, driven by the GUI combo box
    camera      cam;

    // --- Model loading + basic lighting (RENDER_MODE_LIT_MODEL) ---------
    // A loaded model drawn with a single Blinn-Phong directional light.
    // Sliders below are exposed through ImGui so the lighting math is easy
    // to see change live rather than only readable in the shader source.
    model      lit_model;
    rhi_shader lit_shader;
    vec3       albedo;
    vec3       light_dir;          // direction the light travels toward the surface; normalized before use
    vec3       light_color;
    f32        ambient_strength;
    f32        specular_strength;
    f32        shininess;

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

    // --- Model loading + basic lighting setup ---------------------------
    char* lit_vertex_path   = asset_path("shaders/lit.vert");
    char* lit_fragment_path = asset_path("shaders/lit.frag");

    state->lit_shader = rhi_shader_create_from_files(lit_vertex_path, lit_fragment_path);

    free(lit_vertex_path);
    free(lit_fragment_path);

    if (!state->lit_shader)
        FATAL("demo: failed to load lit shader");

    char* model_path = asset_path("models/suzanne.obj");
    state->lit_model  = model_load_obj(model_path);
    free(model_path);

    if (state->lit_model.submesh_count == 0)
        FATAL("demo: failed to load lit demo model");

    state->mode = RENDER_MODE_LIT_MODEL;

    glm_vec3_copy((vec3){ 0.9f, 0.3f, 0.25f }, state->albedo);
    glm_vec3_copy((vec3){ -0.4f, -0.6f, -0.5f }, state->light_dir);
    glm_vec3_copy((vec3){ 1.0f, 1.0f, 1.0f }, state->light_color);
    state->ambient_strength  = 0.12f;
    state->specular_strength = 0.5f;
    state->shininess         = 32.0f;

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

    mat4 view_projection;
    camera_get_view_projection(&state->cam, view_projection);

    if (state->mode == RENDER_MODE_TEXTURED_CUBE)
    {
        mat4 model;
        glm_mat4_identity(model);
        glm_rotate(model, time, (vec3){ 0.3f, 1.0f, 0.2f });

        mat4 mvp;
        glm_mat4_mul(view_projection, model, mvp);

        rhi_shader_set_mat4(state->shader, "u_mvp", (const f32*)mvp);

        rhi_shader_bind(state->shader);
        rhi_texture_bind(state->textures[state->selected_texture], 0);
        rhi_draw_indexed(state->vertex_buffer, state->index_buffer,
                          sizeof(CUBE_INDICES) / sizeof(CUBE_INDICES[0]));
    }
    else // RENDER_MODE_LIT_MODEL
    {
        // Slow spin around Y so the lighting is visibly hitting a changing
        // set of surface normals rather than a single static pose.
        mat4 model;
        glm_mat4_identity(model);

        // Inverse-transpose of the model matrix: transforms normals
        // correctly even under non-uniform scale (a plain model-matrix
        // multiply would skew them). Uniform-scale-only here today, so
        // this is a no-op in practice, but doing it properly means this
        // demo doesn't quietly break the moment someone scales the model.
        mat4 normal_matrix;
        glm_mat4_copy(model, normal_matrix);
        glm_mat4_inv(normal_matrix, normal_matrix);
        glm_mat4_transpose(normal_matrix);

        vec3 light_dir_normalized;
        glm_vec3_normalize_to(state->light_dir, light_dir_normalized);

        rhi_shader_set_mat4(state->lit_shader, "u_model", (const f32*)model);
        rhi_shader_set_mat4(state->lit_shader, "u_view_projection", (const f32*)view_projection);
        rhi_shader_set_mat4(state->lit_shader, "u_normal_matrix", (const f32*)normal_matrix);
        rhi_shader_set_vec3(state->lit_shader, "u_albedo", state->albedo[0], state->albedo[1], state->albedo[2]);
        rhi_shader_set_vec3(state->lit_shader, "u_light_dir",
                             light_dir_normalized[0], light_dir_normalized[1], light_dir_normalized[2]);
        rhi_shader_set_vec3(state->lit_shader, "u_light_color",
                             state->light_color[0], state->light_color[1], state->light_color[2]);
        rhi_shader_set_vec3(state->lit_shader, "u_view_pos",
                             state->cam.position[0], state->cam.position[1], state->cam.position[2]);
        rhi_shader_set_float(state->lit_shader, "u_ambient_strength", state->ambient_strength);
        rhi_shader_set_float(state->lit_shader, "u_specular_strength", state->specular_strength);
        rhi_shader_set_float(state->lit_shader, "u_shininess", state->shininess);

        rhi_shader_bind(state->lit_shader);

        for (uint32 i = 0; i < state->lit_model.submesh_count; ++i)
            mesh_draw(&state->lit_model.submeshes[i].gpu_mesh);
    }

    // App/on_frame runs between imgui_layer_new_frame() and
    // imgui_layer_render() (see App.c), so any ig*() calls here just work.
    // Fullscreen dockspace so ImGui windows can actually be dragged and
    // docked against something. dockspace_id=0 auto-generates an ID; NULL
    // viewport = the main viewport; no flags, no window-class restriction.
    igDockSpaceOverViewport(0, NULL, ImGuiDockNodeFlags_PassthruCentralNode, NULL);

    igBegin("Scene", NULL, 0);

    igText("Render Mode");
    igRadioButton_IntPtr("Textured Cube", (int*)&state->mode, RENDER_MODE_TEXTURED_CUBE);
    igSameLine(0.0f, -1.0f);
    igRadioButton_IntPtr("Lit Model", (int*)&state->mode, RENDER_MODE_LIT_MODEL);
    igSeparator();

    if (state->mode == RENDER_MODE_TEXTURED_CUBE)
    {
        // Texture picker: a combo box bound directly to selected_texture,
        // which is what the draw call above reads every frame -- no extra
        // plumbing needed between "user picks an entry" and "the cube
        // shows it".
        igCombo_Str_arr("Texture", &state->selected_texture, TEXTURE_NAMES, (int)TEXTURE_COUNT, -1);
    }
    else // RENDER_MODE_LIT_MODEL
    {
        // Every slider here is read straight out of state by the lighting
        // block above -- no extra plumbing between "user drags a slider"
        // and "the next frame's draw call uses it".
        igColorEdit3("Albedo", state->albedo, 0);
        igColorEdit3("Light Color", state->light_color, 0);
        igSliderFloat3("Light Direction", state->light_dir, -1.0f, 1.0f, "%.2f", 0);
        igSliderFloat("Ambient", &state->ambient_strength, 0.0f, 1.0f, "%.2f", 0);
        igSliderFloat("Specular", &state->specular_strength, 0.0f, 2.0f, "%.2f", 0);
        igSliderFloat("Shininess", &state->shininess, 1.0f, 256.0f, "%.0f", 0);
    }

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
