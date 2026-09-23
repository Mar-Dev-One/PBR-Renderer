#include "Light.h"

void
light_init_directional(light* l, vec3 direction, vec3 color, f32 intensity)
{
    l->type      = LIGHT_DIRECTIONAL;
    glm_vec3_normalize_to(direction, l->direction);
    glm_vec3_copy(color, l->color);
    l->intensity = intensity;

    l->range               = 0.0f;
    l->inner_cone_degrees  = 0.0f;
    l->outer_cone_degrees  = 0.0f;
}

void
light_init_point(light* l, vec3 color, f32 intensity, f32 range)
{
    l->type      = LIGHT_POINT;
    glm_vec3_zero(l->direction);   // unused by LIGHT_POINT; zeroed rather than left uninitialized
    glm_vec3_copy(color, l->color);
    l->intensity = intensity;

    l->range               = range;
    l->inner_cone_degrees  = 0.0f;
    l->outer_cone_degrees  = 0.0f;
}

void
light_init_spot(light* l, vec3 direction, vec3 color, f32 intensity,
                f32 range, f32 inner_cone_degrees, f32 outer_cone_degrees)
{
    l->type      = LIGHT_SPOT;
    glm_vec3_normalize_to(direction, l->direction);
    glm_vec3_copy(color, l->color);
    l->intensity = intensity;

    l->range              = range;
    l->inner_cone_degrees = inner_cone_degrees;
    l->outer_cone_degrees = outer_cone_degrees;
}
