#pragma once

#include "../Core/Defines.h"
#include "../RHI/RHI.h"

#include <cglm/cglm.h>

// The one vertex layout every model loader (Model.h) and every lit shader
// (assets/shaders/lit.*, pbr.*) agrees on. Add a new attribute here only if
// both sides are updated together -- mesh_create() below wires this exact
// layout up to locations 0/1/2/3, matching the shaders'
// `layout(location = N)` declarations.
//
// `tangent` is xyz = tangent direction, w = handedness (+1/-1), so a
// shader can rebuild the bitangent as cross(normal, tangent.xyz) * tangent.w
// -- the same convention glTF uses. It's a plain float[4] rather than a
// cglm `vec4` so the struct isn't padded up to 16-byte alignment.
//
// UV convention: (0,0) is the *bottom-left* of the image, matching how
// Core/Image.c flips textures on load. Loaders for formats with a
// top-left UV origin (glTF) flip V on the way in.
typedef struct mesh_vertex
{
    vec3 position;
    vec3 normal;
    vec2 uv;
    f32  tangent[4];
} mesh_vertex;

// A single GPU-resident indexed triangle mesh. Deliberately as dumb as
// Camera -- no material/texture reference lives here, that's the caller's
// (Model.h's) responsibility, so Mesh stays reusable for hand-built
// geometry too (procedural shapes, primitives), not just loaded models.
typedef struct mesh
{
    rhi_buffer vertex_buffer;
    rhi_buffer index_buffer;
    uint32     index_count;
    uint32     vertex_count;

    // Object-space axis-aligned bounds, filled in by mesh_create() from the
    // vertex positions. Used for auto-fitting the camera to a loaded model
    // and (later) frustum culling.
    vec3       bounds_min;
    vec3       bounds_max;
} mesh;

// Uploads `vertices`/`indices` to the GPU as RHI_USAGE_STATIC buffers.
// Caller keeps ownership of the input arrays; mesh only holds onto the
// resulting GPU buffers.
mesh mesh_create(const mesh_vertex* vertices, uint32 vertex_count,
                  const uint32* indices, uint32 index_count);

// --- CPU-side vertex helpers (run before mesh_create) --------------------

// Fills in every vertex's `normal` as the area-weighted average of the
// face normals of the triangles it belongs to (smooth shading). Only
// meaningful for indexed meshes where triangles actually share vertices.
void mesh_compute_smooth_normals(mesh_vertex* vertices, uint32 vertex_count,
                                  const uint32* indices, uint32 index_count);

// Fills in every vertex's `tangent` from its UVs (per-triangle tangent/
// bitangent accumulated per vertex, then Gram-Schmidt orthogonalised
// against the normal). Needed for normal mapping when the source file
// doesn't ship tangents. Vertices with degenerate UVs get an arbitrary but
// valid tangent so a normal map never produces NaNs.
void mesh_compute_tangents(mesh_vertex* vertices, uint32 vertex_count,
                            const uint32* indices, uint32 index_count);

void mesh_draw(const mesh* m);

// Safe to call on a zero-initialized mesh (e.g. one that failed to load).
void mesh_destroy(mesh* m);
