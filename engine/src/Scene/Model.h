#pragma once

#include "../Core/Defines.h"
#include "Mesh.h"

// One material group from the source file, already uploaded to the GPU.
// Split by material (rather than merged into one draw call) so a render
// pass can bind the right albedo/textures per submesh later -- today
// nothing reads material_index yet (see testbed/main.c, which just draws
// every submesh with one shared lit-shader material), but the split is
// free at load time and expensive to retrofit afterwards.
typedef struct model_submesh
{
    mesh  gpu_mesh;
    int32 material_index;   // index into model::material_names, or -1 if the file had no materials
} model_submesh;

typedef struct model
{
    model_submesh* submeshes;
    uint32         submesh_count;

    char**         material_names;   // material_count entries, heap-owned
    uint32         material_count;
} model;

// Loads a Wavefront .obj file (via the vendored fast_obj parser) and
// uploads it as one or more GPU meshes, one per material used in the file
// (a single implicit group if the file has no materials at all).
//
// Faces are fan-triangulated from their first corner, which is exact for
// the convex/near-planar polygons .obj exporters produce and is what
// fast_obj itself expects the caller to do. Vertices are not deduplicated
// into a shared-vertex index buffer -- every face corner becomes its own
// vertex, indexed 0..N-1 -- which is simiplicity over minimum GPU memory;
// revisit if a loaded model is ever large enough for that to matter.
//
// Normals come from the file when present; a face with no normal data
// falls back to a flat, per-face normal (computed from its first three
// corners) so lighting still works, just faceted rather than smooth.
//
// Returns a zero-initialized model (submesh_count == 0) if the file can't
// be read or parsed; check that before drawing.
model model_load_obj(const char* path);

// Frees every submesh's GPU buffers and all CPU-side bookkeeping. Safe to
// call on a zero-initialized/failed-load model.
void model_destroy(model* m);
