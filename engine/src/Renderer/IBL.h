#pragma once

#include "../Core/Defines.h"
#include "../RHI/RHI.h"

#include <cglm/cglm.h>

// Image-based lighting: turns an equirectangular HDR panorama into the three
// things a PBR shader needs to light a surface from "everywhere at once", plus
// a skybox to look at it.
//
//   environment  the panorama as a cube map with a mip chain (the sky itself)
//   irradiance   32^2 cube map: diffuse light arriving from each direction
//   prefiltered  128^2 cube map: mip N = the environment blurred by the GGX
//                specular lobe of roughness N / (mip_count - 1)
//
// Baking happens once, on the GPU, inside ibl_create(). The BRDF half of the
// split-sum approximation is not a texture here -- pbr.frag evaluates it with
// an analytic fit (env_brdf_approx).

// Texture units the maps live on when bound for drawing. Material.h owns 0..4
// and the testbed puts the shadow map on 5, so IBL starts at 6.
#define IBL_SLOT_IRRADIANCE   6
#define IBL_SLOT_PREFILTERED  7
#define IBL_SLOT_ENVIRONMENT  8   // only bound while the skybox is drawn

typedef struct ibl_environment
{
    rhi_texture environment;
    rhi_texture irradiance;
    rhi_texture prefiltered;
    uint32      environment_mip_count;
    uint32      prefiltered_mip_count;

    // Skybox drawing resources, kept for the environment's lifetime.
    rhi_buffer  cube_vertices;
    rhi_buffer  cube_indices;
    rhi_shader  skybox_shader;
} ibl_environment;

// Bakes `hdr_path` (an absolute path to a Radiance .hdr, ideally a 2:1
// equirectangular panorama; see Core/Paths.h asset_path()). Needs a live GL
// context, and leaves the viewport / render state as it found them.
//
// Returns false and leaves *ibl zeroed (safe to pass to ibl_destroy and
// ibl_bind) if the file can't be read or a shader fails to build. Call
// ibl_destroy() first if *ibl already holds an environment.
b8   ibl_create(ibl_environment* ibl, const char* hdr_path);

// Safe on a zeroed / failed / already-destroyed environment.
void ibl_destroy(ibl_environment* ibl);

// Per-frame setup for the PBR shader (assets/shaders/pbr.frag): binds the
// irradiance and prefiltered maps to IBL_SLOT_* and sets its u_use_ibl,
// u_irradiance_map, u_prefiltered_map and u_prefiltered_max_lod uniforms.
// Pass ibl = NULL (or a failed environment) to switch the shader back to its
// built-in hemisphere ambient; the sampler uniforms are still pointed at
// their own units so the program stays valid to draw with.
void ibl_bind(const ibl_environment* ibl, rhi_shader pbr_shader);

// Draws the environment as a background at the far plane. Call before the
// scene geometry (blended materials need the sky already behind them).
// `blur` is 0..1: 0 = sharp panorama, 1 = the blurriest mip. Exposure matches
// pbr.frag's u_exposure. Leaves the default render state active afterwards.
void ibl_draw_skybox(const ibl_environment* ibl, const mat4 view, const mat4 projection,
                     f32 exposure, f32 blur);
