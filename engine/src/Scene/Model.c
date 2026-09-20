#include "Model.h"

#include <string.h>

#define FAST_OBJ_IMPLEMENTATION
#include "fast_obj.h"

// Growable scratch buffer of mesh_vertex, one per material group, built up
// while walking the OBJ's faces before a single GPU upload per group.
// Plain realloc-doubling is fine here -- this only runs once per model
// load, not per frame.
typedef struct vertex_list
{
    mesh_vertex* data;
    uint32       count;
    uint32       capacity;
} vertex_list;

static void
vertex_list_push(vertex_list* list, mesh_vertex v)
{
    if (list->count == list->capacity)
    {
        list->capacity = list->capacity ? list->capacity * 2 : 64;
        list->data = realloc(list->data, (uint64)list->capacity * sizeof(mesh_vertex));

        if (!list->data)
            FATAL("model_load_obj: out of memory growing vertex list");
    }

    list->data[list->count++] = v;
}

// malloc+memcpy string copy rather than POSIX strdup, so this doesn't
// depend on which libc extensions happen to be visible under
// -std=c11 on a given compiler.
static char*
duplicate_string(const char* s)
{
    size_t len = strlen(s) + 1;
    char*  copy = malloc(len);

    if (copy)
        memcpy(copy, s, len);

    return copy;
}

// Reads one face corner into a mesh_vertex. `idx.p/t/n` are 1-based into
// the mesh's shared position/texcoord/normal pools, with 0 meaning "not
// present" (see fast_obj.h) -- a missing normal falls back to
// `flat_normal` (the face's own geometric normal), a missing UV to (0,0).
static mesh_vertex
read_vertex(const fastObjMesh* obj, fastObjIndex idx, const vec3 flat_normal)
{
    mesh_vertex v;

    v.position[0] = obj->positions[3 * idx.p + 0];
    v.position[1] = obj->positions[3 * idx.p + 1];
    v.position[2] = obj->positions[3 * idx.p + 2];

    if (idx.n != 0 && obj->normal_count > 0)
    {
        v.normal[0] = obj->normals[3 * idx.n + 0];
        v.normal[1] = obj->normals[3 * idx.n + 1];
        v.normal[2] = obj->normals[3 * idx.n + 2];
    }
    else
    {
        glm_vec3_copy((f32*)flat_normal, v.normal);
    }

    if (idx.t != 0 && obj->texcoord_count > 0)
    {
        v.uv[0] = obj->texcoords[2 * idx.t + 0];
        v.uv[1] = obj->texcoords[2 * idx.t + 1];
    }
    else
    {
        v.uv[0] = 0.0f;
        v.uv[1] = 0.0f;
    }

    return v;
}

// Geometric (flat) normal of a face from its first three corners. Used as
// the per-vertex fallback for any corner whose own normal index is 0.
static void
compute_flat_normal(const fastObjMesh* obj, const fastObjIndex* corners, vec3 out_normal)
{
    vec3 p0 = { obj->positions[3 * corners[0].p + 0], obj->positions[3 * corners[0].p + 1], obj->positions[3 * corners[0].p + 2] };
    vec3 p1 = { obj->positions[3 * corners[1].p + 0], obj->positions[3 * corners[1].p + 1], obj->positions[3 * corners[1].p + 2] };
    vec3 p2 = { obj->positions[3 * corners[2].p + 0], obj->positions[3 * corners[2].p + 1], obj->positions[3 * corners[2].p + 2] };

    vec3 edge1, edge2;
    glm_vec3_sub(p1, p0, edge1);
    glm_vec3_sub(p2, p0, edge2);
    glm_vec3_cross(edge1, edge2, out_normal);

    if (glm_vec3_norm2(out_normal) > 1e-12f)
        glm_vec3_normalize(out_normal);
    else
        glm_vec3_copy((vec3){ 0.0f, 1.0f, 0.0f }, out_normal);   // degenerate (zero-area) face
}

model model_load_obj(const char* path)
{
    model result = { 0 };

    fastObjMesh* obj = fast_obj_read(path);
    if (!obj)
    {
        LOG_ERROR("model_load_obj: failed to read '%s'\n", path);
        return result;
    }

    // A face with no material assigned reports face_materials[i] == 0 the
    // same as a real material slot 0 would, so a file with zero materials
    // is treated as one implicit group rather than a special case below.
    uint32 group_count = obj->material_count > 0 ? obj->material_count : 1;

    vertex_list* groups = calloc(group_count, sizeof(vertex_list));
    if (!groups)
        FATAL("model_load_obj: out of memory allocating material groups");

    uint32 index_offset = 0;

    for (uint32 face = 0; face < obj->face_count; ++face)
    {
        uint32 face_vertex_count = obj->face_vertices[face];
        uint32 material = obj->material_count > 0 ? obj->face_materials[face] : 0;

        vec3 flat_normal = { 0.0f, 1.0f, 0.0f };

        if (face_vertex_count >= 3)
        {
            fastObjIndex first_corners[3] = {
                obj->indices[index_offset + 0],
                obj->indices[index_offset + 1],
                obj->indices[index_offset + 2],
            };

            compute_flat_normal(obj, first_corners, flat_normal);
        }

        // Fan triangulation: (0, i, i+1) for i in [1, face_vertex_count-2].
        // Exact for the convex/near-planar polygons .obj exporters emit;
        // fast_obj intentionally leaves triangulation to the caller.
        for (uint32 i = 1; i + 1 < face_vertex_count; ++i)
        {
            fastObjIndex corners[3] = {
                obj->indices[index_offset + 0],
                obj->indices[index_offset + i],
                obj->indices[index_offset + i + 1],
            };

            for (uint32 c = 0; c < 3; ++c)
                vertex_list_push(&groups[material], read_vertex(obj, corners[c], flat_normal));
        }

        index_offset += face_vertex_count;
    }

    result.submeshes = calloc(group_count, sizeof(model_submesh));
    if (!result.submeshes)
        FATAL("model_load_obj: out of memory allocating submeshes");

    uint32 submesh_count = 0;

    for (uint32 g = 0; g < group_count; ++g)
    {
        if (groups[g].count == 0)
            continue;   // material used in the .mtl but never actually referenced by a face

        // Vertices are already expanded one-per-face-corner (see
        // read_vertex()), so there's nothing to share -- indices are just
        // the identity mapping, kept only because Mesh/RHI always draw
        // indexed.
        uint32* indices = malloc((uint64)groups[g].count * sizeof(uint32));
        if (!indices)
            FATAL("model_load_obj: out of memory allocating index buffer");

        for (uint32 i = 0; i < groups[g].count; ++i)
            indices[i] = i;

        model_submesh* sub = &result.submeshes[submesh_count++];
        sub->gpu_mesh       = mesh_create(groups[g].data, groups[g].count, indices, groups[g].count);
        sub->material_index = obj->material_count > 0 ? (int32)g : -1;

        free(indices);
        free(groups[g].data);
    }

    result.submesh_count = submesh_count;

    if (obj->material_count > 0)
    {
        result.material_names = calloc(obj->material_count, sizeof(char*));
        if (!result.material_names)
            FATAL("model_load_obj: out of memory allocating material names");

        for (uint32 i = 0; i < obj->material_count; ++i)
        {
            const char* name = obj->materials[i].name ? obj->materials[i].name : "unnamed";
            result.material_names[i] = duplicate_string(name);
        }

        result.material_count = obj->material_count;
    }

    free(groups);
    fast_obj_destroy(obj);

    LOG_INFO("model_load_obj: loaded '%s' (%u submesh%s)\n",
              path, submesh_count, submesh_count == 1 ? "" : "es");

    return result;
}

void model_destroy(model* m)
{
    if (!m)
        return;

    for (uint32 i = 0; i < m->submesh_count; ++i)
        mesh_destroy(&m->submeshes[i].gpu_mesh);

    free(m->submeshes);

    for (uint32 i = 0; i < m->material_count; ++i)
        free(m->material_names[i]);

    free(m->material_names);

    memset(m, 0, sizeof(*m));
}
