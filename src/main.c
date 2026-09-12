// RHI_Triangle_Test.c
//
// Exercises the RHI end-to-end by actually rendering something, going
// through the same App -> Renderer -> RHI path production code uses (not
// a separate hand-rolled Window+RHI setup). App.on_frame is the hook: it
// runs once, builds a vertex buffer, index buffer and shader through the
// RHI, draws a colored triangle into an off-screen RHI framebuffer, reads
// the pixels back, checks the result, then tells App to stop.
//
// The shader is loaded from assets/shaders/basic.{vert,frag} via
// rhi_shader_create_from_files() rather than embedded as a string, so
// this test also exercises that path (and ASSETS_ROOT resolution) instead
// of just the RHI's raw-string shader entry point.
//
// Exit code 0 = pass, 1 = fail, so it can be wired into ctest via
// add_test() (see CMakeLists.txt).

#include "../src/App.h"
#include "../src/Core/Defines.h"
#include "../src/Core/Paths.h"
#include "../src/RHI/RHI.h"

#include <string.h>

#define FB_WIDTH  800
#define FB_HEIGHT 600

typedef struct rgba8 { uint8 r, g, b, a; } rgba8;

typedef struct test_context
{
    int failures;
} test_context;

static rgba8
sample(const rgba8* pixels, int x, int y)
{
    // glReadPixels/RHI read back with row 0 = bottom of the image.
    return pixels[y * FB_WIDTH + x];
}

static b8
is_close_to(rgba8 p, uint8 r, uint8 g, uint8 b, int tolerance)
{
    return abs((int)p.r - r) <= tolerance
        && abs((int)p.g - g) <= tolerance
        && abs((int)p.b - b) <= tolerance;
}

// Writes a binary PPM so the render can be inspected visually, independent
// of whether the numeric assertions below pass.
static void
write_ppm(const char* path, const rgba8* pixels)
{
    FILE* f = fopen(path, "wb");
    if (!f)
    {
        LOG_ERROR("Could not open %s for writing\n", path);
        return;
    }

    fprintf(f, "P6\n%d %d\n255\n", FB_WIDTH, FB_HEIGHT);

    // PPM is top-down; our readback is bottom-up (GL convention), so flip.
    for (int y = FB_HEIGHT - 1; y >= 0; --y)
    {
        for (int x = 0; x < FB_WIDTH; ++x)
        {
            rgba8 p = pixels[y * FB_WIDTH + x];
            fputc(p.r, f);
            fputc(p.g, f);
            fputc(p.b, f);
        }
    }

    fclose(f);
    LOG_INFO("Wrote %s\n", path);
}

// App's frame-callback hook (see App.h). Runs once (it sets
// app->should_close = true at the end), inside the normal App/Renderer
// frame that's already been created and cleared by run().
static void on_frame(App* app)
{
    test_context* ctx = (test_context*)app->user_data;

    // --- Geometry: one triangle, position (vec2) + color (vec3) per vertex.
    // Big enough in clip space to comfortably cover the framebuffer center.
    f32 vertices[] = {
        //  x,     y,     r,    g,    b
         0.0f,  0.8f,  1.0f, 0.0f, 0.0f,
        -0.8f, -0.8f,  1.0f, 0.0f, 0.0f,
         0.8f, -0.8f,  1.0f, 0.0f, 0.0f,
    };

    uint32 indices[] = { 0, 1, 2 };

    rhi_vertex_attribute attrs[] = {
        { .location = 0, .component_count = 2, .offset = 0 },
        { .location = 1, .component_count = 3, .offset = 2 * sizeof(f32) },
    };

    rhi_vertex_layout layout = {
        .attributes = attrs,
        .attribute_count = 2,
        .stride = 5 * sizeof(f32)
    };

    rhi_buffer vb = rhi_vertex_buffer_create(vertices, sizeof(vertices), &layout, RHI_USAGE_STATIC);
    rhi_buffer ib = rhi_index_buffer_create(indices, sizeof(indices), RHI_USAGE_STATIC);

    // --- Shader: loaded from assets/shaders/ rather than embedded, so
    // this test also exercises rhi_shader_create_from_files() + asset_path().
    char* vertex_path = asset_path("shaders/basic.vert");
    char* fragment_path = asset_path("shaders/basic.frag");

    rhi_shader shader = rhi_shader_create_from_files(vertex_path, fragment_path);

    free(vertex_path);
    free(fragment_path);

    if (!shader)
    {
        LOG_ERROR("FAIL: rhi_shader_create_from_files failed to load/compile/link the test shader\n");
        ctx->failures++;
        app->should_close = true;
        return;
    }

    // --- Off-screen target: this is what we actually render into and read
    // back, so the test never depends on what ends up on screen.
    rhi_framebuffer_attachment color_attachment = { .format = RHI_FORMAT_RGBA8 };

    rhi_framebuffer_desc fb_desc = {
        .width = FB_WIDTH,
        .height = FB_HEIGHT,
        .color_attachments = &color_attachment,
        .color_attachment_count = 1,
        .has_depth_attachment = false
    };

    rhi_framebuffer fb = rhi_framebuffer_create(fb_desc);

    // --- Render: clear to black, draw a solid red triangle over it.
    rhi_framebuffer_bind(fb);
    rhi_clear(0.0f, 0.0f, 0.0f, 1.0f);
    rhi_shader_bind(shader);
    rhi_draw_indexed(vb, ib, 3);


    // --- Read back and verify.
    rgba8* pixels = malloc((size_t)FB_WIDTH * FB_HEIGHT * sizeof(rgba8));
    rhi_read_pixels(0, 0, FB_WIDTH, FB_HEIGHT, pixels);

    // Hand drawing back to the window's own framebuffer before run()
    // presents this frame -- we're done with the off-screen target.

    // Now render the same triangle to the window
    rhi_framebuffer_bind_default();
    rhi_clear(0.0f, 0.0f, 0.0f, 1.0f);
    rhi_shader_bind(shader);
    rhi_draw_indexed(vb, ib, 3);

    write_ppm("rhi_triangle_test_output.ppm", pixels);

    // Center of the framebuffer sits inside the triangle -> should be red.
    rgba8 center = sample(pixels, FB_WIDTH / 2, FB_HEIGHT / 2);
    if (!is_close_to(center, 255, 0, 0, 10))
    {
        LOG_ERROR("FAIL: center pixel expected ~red(255,0,0), got (%d,%d,%d)\n",
                   center.r, center.g, center.b);
        ctx->failures++;
    }
    else
    {
        LOG_INFO("PASS: center pixel is red, triangle rendered\n");
    }

    // A far corner sits outside the triangle -> should still be the clear
    // color (black), proving the draw call didn't just fill the screen.
    rgba8 corner = sample(pixels, 4, 4);
    if (!is_close_to(corner, 0, 0, 0, 10))
    {
        LOG_ERROR("FAIL: corner pixel expected black clear color, got (%d,%d,%d)\n",
                   corner.r, corner.g, corner.b);
        ctx->failures++;
    }
    else
    {
        LOG_INFO("PASS: corner pixel is background, clear worked and draw is bounded\n");
    }

    // Sanity check the triangle covers a plausible fraction of the
    // framebuffer -- catches a degenerate draw (e.g. 0 or 1 pixels drawn)
    // that the two point-samples above could miss.
    int red_pixel_count = 0;
    for (int i = 0; i < FB_WIDTH * FB_HEIGHT; ++i)
    {
        if (is_close_to(pixels[i], 255, 0, 0, 10))
            red_pixel_count++;
    }

    f32 red_fraction = (f32)red_pixel_count / (f32)(FB_WIDTH * FB_HEIGHT);
    LOG_INFO("Red coverage: %.1f%% of framebuffer\n", red_fraction * 100.0f);

    if (red_fraction < 0.05f || red_fraction > 0.9f)
    {
        LOG_ERROR("FAIL: red coverage %.1f%% is outside the plausible range for this triangle\n",
                   red_fraction * 100.0f);
        ctx->failures++;
    }
    else
    {
        LOG_INFO("PASS: triangle covers a plausible fraction of the framebuffer\n");
    }

    
    /*
    // --- Cleanup.
    free(pixels);
    rhi_framebuffer_destroy(fb);
    rhi_shader_destroy(shader);
    rhi_buffer_destroy(ib);
    rhi_buffer_destroy(vb);

    app->should_close = true; // one frame is all this test needs
    */
}

int main(void)
{
    test_context ctx = { .failures = 0 };

    App app = {
        .name = "RHI Triangle Test",
        .on_frame = on_frame,
        .user_data = &ctx,
        .should_close = false
    };

    run(&app);        // App -> Renderer -> RHI init, window, one frame loop
    terminate(&app);  // RHI shutdown, window destroy

    if (ctx.failures == 0)
    {
        LOG_INFO("=== RHI_Triangle_Test: ALL CHECKS PASSED ===\n");
        return 0;
    }

    LOG_ERROR("=== RHI_Triangle_Test: %d CHECK(S) FAILED ===\n", ctx.failures);
    return 1;
}