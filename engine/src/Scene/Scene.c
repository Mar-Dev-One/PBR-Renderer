#include "Scene.h"

#include <string.h>

#define SCENE_INITIAL_CAPACITY 8

void
scene_init(scene* s)
{
    s->entities        = NULL;
    s->entity_count     = 0;
    s->entity_capacity  = 0;
    s->active_camera    = ENTITY_NO_PARENT;
}

// Doubling-capacity growth: unlike Model.c's grow_array (one-off loads,
// grown once per element and never touched again), a scene is built up
// incrementally over the app's lifetime and may be added to every frame
// (spawning entities at runtime), so an O(n) realloc per add would be the
// wrong tradeoff here.
static void
scene_reserve(scene* s, uint32 min_capacity)
{
    if (s->entity_capacity >= min_capacity)
        return;

    uint32 new_capacity = s->entity_capacity ? s->entity_capacity * 2 : SCENE_INITIAL_CAPACITY;
    if (new_capacity < min_capacity)
        new_capacity = min_capacity;

    entity* grown = realloc(s->entities, (uint64)new_capacity * sizeof(entity));
    if (!grown)
        FATAL("scene: out of memory growing entity list");

    s->entities        = grown;
    s->entity_capacity = new_capacity;
}

static int32
scene_add_entity(scene* s, entity_kind kind, const mat4 local_transform, int32 parent)
{
    if (parent != ENTITY_NO_PARENT && (uint32)parent >= s->entity_count)
    {
        LOG_ERROR("scene: parent index %d is out of range (entity_count = %u)\n", parent, s->entity_count);
        return ENTITY_NO_PARENT;
    }

    scene_reserve(s, s->entity_count + 1);

    entity* e = &s->entities[s->entity_count];
    memset(e, 0, sizeof(*e));

    e->name[0] = '\0';
    e->kind    = kind;
    e->parent  = parent;

    if (local_transform)
        glm_mat4_copy((vec4*)local_transform, e->local_transform);
    else
        glm_mat4_identity(e->local_transform);

    glm_mat4_identity(e->world_transform);   // stale until scene_update_world_transforms(); identity beats garbage

    return (int32)s->entity_count++;
}

int32
scene_add_model(scene* s, model* m, const mat4 local_transform, int32 parent)
{
    int32 index = scene_add_entity(s, ENTITY_MODEL, local_transform, parent);
    if (index != ENTITY_NO_PARENT)
        s->entities[index].as_model = m;
    return index;
}

int32
scene_add_light(scene* s, light l, const mat4 local_transform, int32 parent)
{
    int32 index = scene_add_entity(s, ENTITY_LIGHT, local_transform, parent);
    if (index != ENTITY_NO_PARENT)
        s->entities[index].as_light = l;
    return index;
}

int32
scene_add_camera(scene* s, camera* cam, const mat4 local_transform, int32 parent)
{
    int32 index = scene_add_entity(s, ENTITY_CAMERA, local_transform, parent);
    if (index != ENTITY_NO_PARENT)
        s->entities[index].as_camera = cam;
    return index;
}

void
scene_set_active_camera(scene* s, int32 index)
{
    if (index != ENTITY_NO_PARENT
        && ((uint32)index >= s->entity_count || s->entities[index].kind != ENTITY_CAMERA))
    {
        LOG_ERROR("scene: entity %d is not a camera, active_camera left unchanged\n", index);
        return;
    }

    s->active_camera = index;
}

void
scene_update_world_transforms(scene* s)
{
    for (uint32 i = 0; i < s->entity_count; ++i)
    {
        entity* e = &s->entities[i];

        if (e->parent == ENTITY_NO_PARENT)
            glm_mat4_copy(e->local_transform, e->world_transform);
        else
            glm_mat4_mul(s->entities[e->parent].world_transform, e->local_transform, e->world_transform);
    }
}

void
scene_draw_models(const scene* s, rhi_shader shader)
{
    for (uint32 i = 0; i < s->entity_count; ++i)
    {
        const entity* e = &s->entities[i];
        if (e->kind == ENTITY_MODEL && e->as_model)
            model_draw(e->as_model, shader, e->world_transform);
    }
}

void
scene_draw_models_depth(const scene* s, rhi_shader shader, const mat4 light_view_projection)
{
    for (uint32 i = 0; i < s->entity_count; ++i)
    {
        const entity* e = &s->entities[i];
        if (e->kind == ENTITY_MODEL && e->as_model)
            model_draw_depth(e->as_model, shader, e->world_transform, light_view_projection);
    }
}

uint32
scene_count_lights(const scene* s)
{
    uint32 count = 0;
    for (uint32 i = 0; i < s->entity_count; ++i)
        count += (s->entities[i].kind == ENTITY_LIGHT);
    return count;
}

uint32
scene_gather_lights(const scene* s, light* out_lights, vec3* out_world_positions, uint32 max_lights)
{
    uint32 written = 0;

    for (uint32 i = 0; i < s->entity_count && written < max_lights; ++i)
    {
        const entity* e = &s->entities[i];
        if (e->kind != ENTITY_LIGHT)
            continue;

        light l = e->as_light;

        // local_transform/world_transform rotate but (for directional
        // lights) don't translate the stored direction -- mat3 from the
        // mat4 drops translation, which is exactly what's wanted here.
        if (l.type == LIGHT_DIRECTIONAL || l.type == LIGHT_SPOT)
        {
            mat3 rotation;
            glm_mat4_pick3((vec4*)e->world_transform, rotation);
            glm_mat3_mulv(rotation, l.direction, l.direction);
            glm_vec3_normalize(l.direction);
        }

        out_lights[written] = l;

        if (out_world_positions && (l.type == LIGHT_POINT || l.type == LIGHT_SPOT))
        {
            glm_vec3_copy((vec3){ e->world_transform[3][0], e->world_transform[3][1], e->world_transform[3][2] },
                          out_world_positions[written]);
        }

        ++written;
    }

    return written;
}

void
scene_destroy(scene* s)
{
    free(s->entities);
    s->entities        = NULL;
    s->entity_count     = 0;
    s->entity_capacity  = 0;
    s->active_camera    = ENTITY_NO_PARENT;
}
