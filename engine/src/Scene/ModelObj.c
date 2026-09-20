#include "ModelInternal.h"

#include <math.h>
#include <string.h>

#define FAST_OBJ_IMPLEMENTATION
#include "fast_obj.h"

// ---------------------------------------------------------------------------
// Per-material mesh builder: a growing vertex array + index array, plus a
// hash map from (position, uv, normal) index triples to the vertex already
// emitted for them, so a corner shared by six triangles becomes one vertex
// instead of six.
// ---------------------------------------------------------------------------

typedef struct vertex_key
{
    uint32 p, t, n;
} vertex_key;

typedef struct map_slot
{
    vertex_key key;
    uint32     vertex;   // MAP_EMPTY = unused slot
} map_slot;

#define MAP_EMPTY 0xFFFFFFFFu

typedef struct mesh_builder
{
    mesh_vertex* vertices;
    uint32       vertex_count;
    uint32       vertex_capacity;

    uint32*      indices;
    uint32       index_count;
    uint32       index_capacity;

    map_slot*    slots;          // open-addressing table, capacity is a power of two
    uint32       slot_capacity;
    uint32       slot_used;
} mesh_builder;

static uint32
hash_key(vertex_key k)
{
    uint32 h = k.p * 73856093u ^ k.t * 19349663u ^ k.n * 83492791u;
    h ^= h >> 15;
    h *= 0x2c1b3c6du;
    h ^= h >> 12;
    return h;
}

static void
map_rehash(mesh_builder* b, uint32 new_capacity)
{
    map_slot* slots = malloc((uint64)new_capacity * sizeof(map_slot));
    if (!slots)
        FATAL("model_load_obj: out of memory growing vertex map");

    for (uint32 i = 0; i < new_capacity; ++i)
        slots[i].vertex = MAP_EMPTY;

    for (uint32 i = 0; i < b->slot_capacity; ++i)
    {
        if (b->slots[i].vertex == MAP_EMPTY)
            continue;

        uint32 pos = hash_key(b->slots[i].key) & (new_capacity - 1);
        while (slots[pos].vertex != MAP_EMPTY)
            pos = (pos + 1) & (new_capacity - 1);

        slots[pos] = b->slots[i];
    }

    free(b->slots);
    b->slots         = slots;
    b->slot_capacity = new_capacity;
}

// Appends a vertex and returns its index.
static uint32
builder_push_vertex(mesh_builder* b, const mesh_vertex* v)
{
    if (b->vertex_count == b->vertex_capacity)
    {
        b->vertex_capacity = b->vertex_capacity ? b->vertex_capacity * 2 : 64;
        b->vertices = realloc(b->vertices, (uint64)b->vertex_capacity * sizeof(mesh_vertex));
        if (!b->vertices)
            FATAL("model_load_obj: out of memory growing vertex list");
    }

    b->vertices[b->vertex_count] = *v;
    return b->vertex_count++;
}

static void
builder_push_index(mesh_builder* b, uint32 index)
{
    if (b->index_count == b->index_capacity)
    {
        b->index_capacity = b->index_capacity ? b->index_capacity * 2 : 192;
        b->indices = realloc(b->indices, (uint64)b->index_capacity * sizeof(uint32));
        if (!b->indices)
            FATAL("model_load_obj: out of memory growing index list");
    }

    b->indices[b->index_count++] = index;
}

// Returns the vertex index for `key`, adding the caller-built vertex `*v`
// only when the key isn't in the map yet.
// `dedupe == false` skips the map entirely -- used for corners whose
// vertex depends on the face they belong to (flat-normal fallback).
static uint32
builder_get_vertex(mesh_builder* b, vertex_key key, b8 dedupe, const mesh_vertex* v)
{
    if (!dedupe)
        return builder_push_vertex(b, v);

    if (b->slot_used * 2 >= b->slot_capacity)
        map_rehash(b, b->slot_capacity ? b->slot_capacity * 2 : 256);

    uint32 mask = b->slot_capacity - 1;
    uint32 pos  = hash_key(key) & mask;

    while (b->slots[pos].vertex != MAP_EMPTY)
    {
        vertex_key* k = &b->slots[pos].key;
        if (k->p == key.p && k->t == key.t && k->n == key.n)
            return b->slots[pos].vertex;

        pos = (pos + 1) & mask;
    }

    uint32 index = builder_push_vertex(b, v);
    b->slots[pos].key    = key;
    b->slots[pos].vertex = index;
    b->slot_used++;
    return index;
}

static void
builder_free(mesh_builder* b)
{
    free(b->vertices);
    free(b->indices);
    free(b->slots);
    memset(b, 0, sizeof(*b));
}

// ---------------------------------------------------------------------------
// OBJ -> vertices
// ---------------------------------------------------------------------------

static void
read_position(const fastObjMesh* obj, fastObjUInt p, vec3 out)
{
    out[0] = obj->positions[3 * p + 0];
    out[1] = obj->positions[3 * p + 1];
    out[2] = obj->positions[3 * p + 2];
}

// Reads one face corner into a mesh_vertex. p/t/n are 1-based into the
// file's shared pools, with 0 meaning "not present" (see fast_obj.h). A
// missing normal falls back to the face's own geometric normal, a missing
// UV to (0,0). Tangents are filled in later by mesh_compute_tangents().
static mesh_vertex
read_vertex(const fastObjMesh* obj, fastObjIndex idx, const vec3 flat_normal)
{
    mesh_vertex v = { 0 };

    read_position(obj, idx.p, v.position);

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

    return v;
}

static void
compute_flat_normal(const fastObjMesh* obj, const fastObjIndex* corners, vec3 out_normal)
{
    vec3 p0, p1, p2, e1, e2;
    read_position(obj, corners[0].p, p0);
    read_position(obj, corners[1].p, p1);
    read_position(obj, corners[2].p, p2);

    glm_vec3_sub(p1, p0, e1);
    glm_vec3_sub(p2, p0, e2);
    glm_vec3_cross(e1, e2, out_normal);

    if (glm_vec3_norm2(out_normal) > 1e-12f)
        glm_vec3_normalize(out_normal);
    else
        glm_vec3_copy((vec3){ 0.0f, 1.0f, 0.0f }, out_normal);   // zero-area face
}

// ---------------------------------------------------------------------------
// OBJ materials -> material
// ---------------------------------------------------------------------------

// Loads (once) and caches the image behind obj->textures[obj_texture].
// Returns NULL for "no texture" (index 0) or a file that won't decode.
static rhi_texture
obj_get_texture(model* result, const fastObjMesh* obj, unsigned int obj_texture,
                rhi_texture* cache, b8 srgb)
{
    if (obj_texture == 0 || obj_texture >= obj->texture_count)
        return NULL;

    if (cache[obj_texture])
        return cache[obj_texture];

    const char* path = obj->textures[obj_texture].path;
    if (!path)
        return NULL;

    image img = image_load(path, 4);
    if (!img.pixels)
    {
        LOG_WARN("model_load_obj: texture '%s' could not be loaded; material will render untextured\n", path);
        return NULL;
    }

    rhi_texture texture = model_upload_image(&img, srgb, RHI_FILTER_LINEAR, RHI_WRAP_REPEAT);
    image_free(&img);

    cache[obj_texture] = texture;
    model_add_texture(result, texture);
    return texture;
}

static void
convert_obj_material(model* result, const fastObjMesh* obj, const fastObjMaterial* src,
                     rhi_texture* srgb_cache, material* out)
{
    material_init_default(out);
    out->name = model_duplicate_string(src->name ? src->name : "unnamed");

    out->base_color[0] = src->Kd[0];
    out->base_color[1] = src->Kd[1];
    out->base_color[2] = src->Kd[2];
    out->base_color[3] = src->d;

    out->emissive[0] = src->Ke[0];
    out->emissive[1] = src->Ke[1];
    out->emissive[2] = src->Ke[2];

    // Blinn-Phong shininess -> GGX roughness (the usual  roughness^2 = 2/(Ns+2) mapping).
    out->roughness = sqrtf(2.0f / (fmaxf(src->Ns, 0.0f) + 2.0f));
    out->metallic  = 0.0f;

    if (src->d < 1.0f)
        out->alpha_mode = MATERIAL_ALPHA_BLEND;

    out->textures[MATERIAL_SLOT_BASE_COLOR] = obj_get_texture(result, obj, src->map_Kd, srgb_cache, true);
    out->textures[MATERIAL_SLOT_EMISSIVE]   = obj_get_texture(result, obj, src->map_Ke, srgb_cache, true);

    // Some exporters write "Kd 0 0 0" next to a map_Kd, expecting the map
    // alone to define the color. Multiplying by a black factor would render
    // the model pitch black, so treat that combination as "no tint".
    if (out->textures[MATERIAL_SLOT_BASE_COLOR] &&
        src->Kd[0] == 0.0f && src->Kd[1] == 0.0f && src->Kd[2] == 0.0f)
    {
        out->base_color[0] = out->base_color[1] = out->base_color[2] = 1.0f;
    }

    // Same idea for emission: a texture with no Ke means "not emissive".
    if (out->textures[MATERIAL_SLOT_EMISSIVE] &&
        src->Ke[0] == 0.0f && src->Ke[1] == 0.0f && src->Ke[2] == 0.0f)
    {
        out->emissive[0] = out->emissive[1] = out->emissive[2] = 1.0f;
    }

    if (src->map_bump != 0)
        LOG_WARN("model_load_obj: material '%s' has a bump map, which .obj can't distinguish "
                 "from a normal map -- ignored (use glTF for normal-mapped models)\n", out->name);
}

// ---------------------------------------------------------------------------
// Loader
// ---------------------------------------------------------------------------

model model_load_obj(const char* path)
{
    model result = { 0 };

    fastObjMesh* obj = fast_obj_read(path);
    if (!obj)
    {
        LOG_ERROR("model_load_obj: failed to read '%s'\n", path);
        return result;
    }

    material_init_default(&result.default_material);

    // A file with no materials is one implicit group; face_materials[i] is
    // ignored in that case.
    uint32 group_count = obj->material_count > 0 ? obj->material_count : 1;

    mesh_builder* groups = calloc(group_count, sizeof(mesh_builder));
    if (!groups)
        FATAL("model_load_obj: out of memory allocating material groups");

    uint32 index_offset = 0;

    for (uint32 face = 0; face < obj->face_count; ++face)
    {
        uint32 face_vertex_count = obj->face_vertices[face];
        uint32 group = obj->material_count > 0 ? obj->face_materials[face] : 0;

        if (group >= group_count)
            group = 0;   // defensive: malformed usemtl bookkeeping

        vec3 flat_normal = { 0.0f, 1.0f, 0.0f };

        if (face_vertex_count >= 3)
        {
            fastObjIndex first[3] = {
                obj->indices[index_offset + 0],
                obj->indices[index_offset + 1],
                obj->indices[index_offset + 2],
            };
            compute_flat_normal(obj, first, flat_normal);
        }

        // Fan triangulation: (0, i, i+1). Exact for the convex/near-planar
        // polygons .obj exporters emit; fast_obj leaves it to the caller.
        for (uint32 i = 1; i + 1 < face_vertex_count; ++i)
        {
            fastObjIndex corners[3] = {
                obj->indices[index_offset + 0],
                obj->indices[index_offset + i],
                obj->indices[index_offset + i + 1],
            };

            for (uint32 c = 0; c < 3; ++c)
            {
                mesh_vertex v = read_vertex(obj, corners[c], flat_normal);

                // Corners that have their own normal can be welded with any
                // other corner naming the same p/t/n. Corners relying on the
                // per-face fallback normal can't (the normal differs per face).
                b8 has_normal = corners[c].n != 0 && obj->normal_count > 0;
                vertex_key key = { corners[c].p, corners[c].t, corners[c].n };

                builder_push_index(&groups[group],
                                   builder_get_vertex(&groups[group], key, has_normal, &v));
            }
        }

        index_offset += face_vertex_count;
    }

    // Textures are shared between materials that name the same file.
    rhi_texture* texture_cache = calloc((uint64)obj->texture_count + 1, sizeof(rhi_texture));
    if (!texture_cache)
        FATAL("model_load_obj: out of memory allocating texture cache");

    // group index -> material index in `result` (-1 if the file has none).
    int32* material_of_group = malloc(group_count * sizeof(int32));
    if (!material_of_group)
        FATAL("model_load_obj: out of memory");

    for (uint32 g = 0; g < group_count; ++g)
    {
        material_of_group[g] = -1;

        if (obj->material_count == 0 || groups[g].index_count == 0)
            continue;   // no materials in the file, or one never used by any face

        material converted;
        convert_obj_material(&result, obj, &obj->materials[g], texture_cache, &converted);
        material_of_group[g] = (int32)model_add_material(&result, &converted);
    }

    mat4 identity;
    glm_mat4_identity(identity);

    for (uint32 g = 0; g < group_count; ++g)
    {
        mesh_builder* b = &groups[g];
        if (b->index_count == 0)
            continue;

        mesh_compute_tangents(b->vertices, b->vertex_count, b->indices, b->index_count);

        mesh gpu = mesh_create(b->vertices, b->vertex_count, b->indices, b->index_count);
        uint32 mesh_index = model_add_mesh(&result, gpu);
        model_add_submesh(&result, mesh_index, material_of_group[g], identity);

        builder_free(b);
    }

    free(material_of_group);
    free(texture_cache);
    free(groups);
    fast_obj_destroy(obj);

    model_finalize(&result);

    LOG_INFO("model_load_obj: loaded '%s' (%u submeshes, %u materials, %u textures, %u vertices, %u triangles)\n",
             path, result.submesh_count, result.material_count, result.texture_count,
             result.vertex_count, result.triangle_count);

    return result;
}
