#version 460 core

// Cascade count must match Renderer/Shadow.h's CSM_MAX_CASCADES -- no
// shared header between C and GLSL here, so the two are defined separately
// and must be edited together (same reasoning as Lighting.h's
// LIGHTING_MAX_LIGHTS note).
#define CSM_MAX_CASCADES 4

// Vertex layout is Scene/Mesh.h's mesh_vertex.
layout(location = 0) in vec3 in_pos;
layout(location = 1) in vec3 in_normal;
layout(location = 2) in vec2 in_uv;
layout(location = 3) in vec4 in_tangent;   // xyz = tangent, w = bitangent handedness (+1/-1)

out vec3 v_world_pos;
out vec3 v_normal;
out vec4 v_tangent;
out vec2 v_uv;
out vec4 v_light_space_pos[CSM_MAX_CASCADES];
out float v_view_depth;

uniform mat4 u_model;
uniform mat4 u_view;                   // camera view only (no projection) -- just for v_view_depth below
uniform mat4 u_view_projection;
uniform mat4 u_normal_matrix;          // inverse-transpose of u_model; only the upper 3x3 is read

// Every cascade's light view * projection, same cameras csm_render() (see
// Renderer/Shadow.h) rendered each shadow map with. Computing all
// CSM_MAX_CASCADES here (rather than just the u_cascade_count that are
// actually in use) costs a few unused mat4 multiplies but keeps this a
// plain unrolled loop instead of a uniform-controlled one; cascade
// selection itself happens in pbr.frag, per fragment.
uniform mat4 u_light_view_projection[CSM_MAX_CASCADES];

void main()
{
    vec4 world_pos = u_model * vec4(in_pos, 1.0);

    v_world_pos = world_pos.xyz;
    v_normal    = mat3(u_normal_matrix) * in_normal;

    // Tangents are directions along the surface, so they transform with the
    // model matrix itself (not the inverse-transpose that normals need).
    v_tangent   = vec4(mat3(u_model) * in_tangent.xyz, in_tangent.w);
    v_uv        = in_uv;

    // Where this vertex lands in each cascade's shadow map -- an affine
    // transform of world_pos, so computing it per-vertex and interpolating
    // (instead of per-fragment) gives the identical result for free.
    for (int i = 0; i < CSM_MAX_CASCADES; ++i)
        v_light_space_pos[i] = u_light_view_projection[i] * world_pos;

    // Distance along the camera's forward axis, used by pbr.frag to pick
    // which cascade a fragment falls into. Also affine in world_pos, same
    // reasoning as above.
    v_view_depth = -(u_view * world_pos).z;

    gl_Position = u_view_projection * world_pos;
}
