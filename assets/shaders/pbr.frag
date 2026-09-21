#version 460 core

// Metallic-roughness PBR (Cook-Torrance GGX) for models loaded through
// Scene/Model.h. Direct light is one directional light; indirect light is
// image-based (Renderer/IBL.h: baked irradiance + GGX-prefiltered environment)
// when u_use_ibl is set, and otherwise a cheap sky/ground hemisphere -- enough
// that metals aren't pitch black while no environment is loaded.
// Output is tone-mapped and gamma-encoded, so it goes straight to the
// (non-sRGB) default framebuffer.

in vec3 v_world_pos;
in vec3 v_normal;
in vec4 v_tangent;
in vec2 v_uv;
in vec4 v_light_space_pos;

out vec4 frag_color;

// --- Material (set by material_bind, see Scene/Material.h) ------------------
uniform vec4  u_base_color;
uniform float u_metallic;
uniform float u_roughness;
uniform vec3  u_emissive;
uniform float u_normal_scale;
uniform float u_occlusion_strength;
uniform int   u_alpha_mode;        // 0 opaque, 1 mask, 2 blend
uniform float u_alpha_cutoff;
uniform int   u_texture_mask;      // bit N set = material slot N has a texture

uniform sampler2D u_base_color_tex;          // slot 0, sRGB
uniform sampler2D u_metallic_roughness_tex;  // slot 1, G = roughness, B = metallic
uniform sampler2D u_normal_tex;              // slot 2
uniform sampler2D u_occlusion_tex;           // slot 3, R = occlusion
uniform sampler2D u_emissive_tex;            // slot 4, sRGB

// --- Scene -------------------------------------------------------------------
uniform vec3  u_light_dir;         // direction the light travels, normalized
uniform vec3  u_light_color;
uniform float u_light_intensity;
uniform vec3  u_view_pos;
uniform float u_ambient_strength;
uniform float u_exposure;

uniform sampler2D u_shadow_map;    // depth-only, rendered by model_draw_depth() from the light's view

// --- Image-based lighting (set by ibl_bind, see Renderer/IBL.h) ---------------
uniform int         u_use_ibl;              // 0 = hemisphere fallback below
uniform samplerCube u_irradiance_map;       // slot 6, diffuse irradiance / pi
uniform samplerCube u_prefiltered_map;      // slot 7, mip N = GGX-blurred at roughness N / max_lod
uniform float       u_prefiltered_max_lod;  // last mip index of u_prefiltered_map

const float PI = 3.14159265359;

bool has_texture(int slot) { return (u_texture_mask & (1 << slot)) != 0; }

// GGX / Trowbridge-Reitz normal distribution.
float distribution_ggx(float NoH, float alpha)
{
    float a2 = alpha * alpha;
    float d  = NoH * NoH * (a2 - 1.0) + 1.0;
    return a2 / (PI * d * d);
}

// Height-correlated Smith visibility term (already includes the 1/(4 NoL NoV)).
float visibility_smith_ggx(float NoV, float NoL, float alpha)
{
    float a2 = alpha * alpha;
    float gv = NoL * sqrt(NoV * NoV * (1.0 - a2) + a2);
    float gl = NoV * sqrt(NoL * NoL * (1.0 - a2) + a2);
    return 0.5 / max(gv + gl, 1e-5);
}

vec3 fresnel_schlick(float VoH, vec3 f0)
{
    return f0 + (1.0 - f0) * pow(clamp(1.0 - VoH, 0.0, 1.0), 5.0);
}

// Analytic fit of the split-sum environment BRDF (Karis, mobile PBR) --
// avoids needing a lookup texture.
vec3 env_brdf_approx(vec3 specular_color, float roughness, float NoV)
{
    const vec4 c0 = vec4(-1.0, -0.0275, -0.572,  0.022);
    const vec4 c1 = vec4( 1.0,  0.0425,  1.040, -0.04);
    vec4  r = roughness * c0 + c1;
    float a004 = min(r.x * r.x, exp2(-9.28 * NoV)) * r.x + r.y;
    vec2  ab = vec2(-1.04, 1.04) * a004 + r.zw;
    return specular_color * ab.x + ab.y;
}

// PCF-filtered shadow test. Returns 0 = fully lit .. 1 = fully shadowed.
// `NoL` drives a slope-scaled bias: surfaces nearly edge-on to the light
// need more bias than ones facing it head-on, or they self-shadow in
// stripes ("shadow acne").
float shadow_calculation(vec4 light_space_pos, float NoL)
{
    // Perspective divide -- a no-op for the orthographic light projection
    // used today, but harmless and keeps this correct if that ever changes
    // to a spot light's perspective projection.
    vec3 proj = light_space_pos.xyz / light_space_pos.w;
    proj = proj * 0.5 + 0.5;   // clip space [-1,1] -> depth-map/UV space [0,1]

    // Outside the light's frustum (or beyond its far plane): nothing known
    // about occluders there, so don't shadow it.
    if (proj.z > 1.0 || proj.x < 0.0 || proj.x > 1.0 || proj.y < 0.0 || proj.y > 1.0)
        return 0.0;

    float bias = max(0.0025 * (1.0 - NoL), 0.0006);

    vec2  texel  = 1.0 / vec2(textureSize(u_shadow_map, 0));
    float shadow = 0.0;

    for (int x = -1; x <= 1; ++x)
    {
        for (int y = -1; y <= 1; ++y)
        {
            float closest_depth = texture(u_shadow_map, proj.xy + vec2(x, y) * texel).r;
            shadow += (proj.z - bias > closest_depth) ? 1.0 : 0.0;
        }
    }

    return shadow / 9.0;
}

vec3 hemisphere(vec3 dir)
{
    const vec3 sky    = vec3(0.55, 0.65, 0.85);
    const vec3 ground = vec3(0.22, 0.20, 0.18);
    return mix(ground, sky, dir.y * 0.5 + 0.5);
}

vec3 aces_tonemap(vec3 x)
{
    return clamp((x * (2.51 * x + 0.03)) / (x * (2.43 * x + 0.59) + 0.14), 0.0, 1.0);
}

void main()
{
    // --- Surface inputs ------------------------------------------------------
    vec4 base = u_base_color;
    if (has_texture(0))
        base *= texture(u_base_color_tex, v_uv);

    if (u_alpha_mode == 1 && base.a < u_alpha_cutoff)
        discard;

    float roughness = u_roughness;
    float metallic  = u_metallic;
    if (has_texture(1))
    {
        vec3 mr = texture(u_metallic_roughness_tex, v_uv).rgb;
        roughness *= mr.g;
        metallic  *= mr.b;
    }
    roughness = clamp(roughness, 0.045, 1.0);   // a perfect mirror makes the highlight vanish
    metallic  = clamp(metallic, 0.0, 1.0);

    float occlusion = 1.0;
    if (has_texture(3))
        occlusion = 1.0 + u_occlusion_strength * (texture(u_occlusion_tex, v_uv).r - 1.0);

    vec3 emissive = u_emissive;
    if (has_texture(4))
        emissive *= texture(u_emissive_tex, v_uv).rgb;

    // --- Shading normal ------------------------------------------------------
    vec3 N = normalize(v_normal);
    if (!gl_FrontFacing)
        N = -N;   // double-sided materials: light the back face as its own front

    if (has_texture(2))
    {
        vec3 T = normalize(v_tangent.xyz - N * dot(N, v_tangent.xyz));   // re-orthogonalise after interpolation
        vec3 B = cross(N, T) * v_tangent.w;

        vec3 tn = texture(u_normal_tex, v_uv).xyz * 2.0 - 1.0;
        tn.xy *= u_normal_scale;

        N = normalize(mat3(T, B, N) * tn);
    }

    vec3 V = normalize(u_view_pos - v_world_pos);
    float NoV = max(dot(N, V), 1e-4);

    vec3 diffuse_color = base.rgb * (1.0 - metallic);
    vec3 f0 = mix(vec3(0.04), base.rgb, metallic);
    float alpha = roughness * roughness;

    // --- Direct light ----------------------------------------------------------
    vec3  L   = normalize(-u_light_dir);
    vec3  H   = normalize(V + L);
    float NoL = max(dot(N, L), 0.0);
    float NoH = max(dot(N, H), 0.0);
    float VoH = max(dot(V, H), 0.0);

    vec3 F = fresnel_schlick(VoH, f0);
    vec3 specular = distribution_ggx(NoH, alpha) * visibility_smith_ggx(NoV, NoL, alpha) * F;
    vec3 diffuse  = (1.0 - F) * diffuse_color / PI;

    float shadow = shadow_calculation(v_light_space_pos, NoL);
    vec3  direct = (diffuse + specular) * u_light_color * u_light_intensity * NoL * (1.0 - shadow);

    // --- Ambient --------------------------------------------------------------------
    vec3 R = reflect(-V, N);

    vec3 irradiance;
    vec3 reflection;
    if (u_use_ibl != 0)
    {
        irradiance = texture(u_irradiance_map, N).rgb;
        reflection = textureLod(u_prefiltered_map, R, roughness * u_prefiltered_max_lod).rgb;
    }
    else
    {
        // Hemisphere stand-in used until an environment is loaded.
        irradiance = hemisphere(N);
        reflection = mix(hemisphere(R), irradiance, roughness);   // rougher = blurrier = closer to the average
    }

    vec3 ambient = (diffuse_color * irradiance + reflection * env_brdf_approx(f0, roughness, NoV))
                 * occlusion * u_ambient_strength;

    vec3 color = direct + ambient + emissive;

    color = aces_tonemap(color * u_exposure);
    color = pow(color, vec3(1.0 / 2.2));

    frag_color = vec4(color, u_alpha_mode == 2 ? base.a : 1.0);
}
