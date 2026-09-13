#include "Camera.h"

void camera_init(camera* cam,
                  vec3 position,
                  vec3 target,
                  f32 fov_degrees,
                  f32 aspect,
                  f32 near_plane,
                  f32 far_plane)
{
    glm_vec3_copy(position, cam->position);
    glm_vec3_copy(target, cam->target);
    glm_vec3_copy((vec3){ 0.0f, 1.0f, 0.0f }, cam->up);

    cam->fov_degrees = fov_degrees;
    cam->aspect      = aspect;
    cam->near_plane  = near_plane;
    cam->far_plane   = far_plane;
}

void camera_set_aspect(camera* cam, f32 aspect)
{
    cam->aspect = aspect;
}

void camera_get_view(const camera* cam, mat4 out_view)
{
    glm_lookat((f32*)cam->position, (f32*)cam->target, (f32*)cam->up, out_view);
}

void camera_get_projection(const camera* cam, mat4 out_projection)
{
    glm_perspective(glm_rad(cam->fov_degrees), cam->aspect,
                     cam->near_plane, cam->far_plane, out_projection);
}

void camera_get_view_projection(const camera* cam, mat4 out_view_projection)
{
    mat4 view, projection;
    camera_get_view(cam, view);
    camera_get_projection(cam, projection);

    glm_mat4_mul(projection, view, out_view_projection);
}
