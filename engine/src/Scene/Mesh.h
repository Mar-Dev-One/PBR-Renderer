#pragma once

#include "../Core/Defines.h"
#include "../RHI/RHI.h"

#include <cglm/cglm.h>

// The one vertex layout every model loader (Model.h) and every lit shader
// (assets/shaders/lit.*) agrees on. Add a new attribute here only if both
// sides are updated together -- mesh_create() below wires this exact
// layout up to locations 0/1/2, matching lit.vert's `layout(location = N)`
// declarations.
typedef struct mesh_vertex
{
    vec3 position;
    vec3 normal;
    vec2 uv;
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
} mesh;

// Uploads `vertices`/`indices` to the GPU as RHI_USAGE_STATIC buffers.
// Caller keeps ownership of the input arrays; mesh only holds onto the
// resulting GPU buffers.
mesh mesh_create(const mesh_vertex* vertices, uint32 vertex_count,
                  const uint32* indices, uint32 index_count);

void mesh_draw(const mesh* m);

// Safe to call on a zero-initialized mesh (e.g. one that failed to load).
void mesh_destroy(mesh* m);
