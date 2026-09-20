#include "Mesh.h"

#include <stddef.h>

mesh mesh_create(const mesh_vertex* vertices, uint32 vertex_count,
                  const uint32* indices, uint32 index_count)
{
    rhi_vertex_attribute attrs[] = {
        { .location = 0, .component_count = 3, .offset = offsetof(mesh_vertex, position) },
        { .location = 1, .component_count = 3, .offset = offsetof(mesh_vertex, normal) },
        { .location = 2, .component_count = 2, .offset = offsetof(mesh_vertex, uv) },
    };

    rhi_vertex_layout layout = {
        .attributes = attrs,
        .attribute_count = 3,
        .stride = sizeof(mesh_vertex)
    };

    mesh m = { 0 };

    m.vertex_buffer = rhi_vertex_buffer_create(vertices, (uint64)vertex_count * sizeof(mesh_vertex),
                                                &layout, RHI_USAGE_STATIC);
    m.index_buffer  = rhi_index_buffer_create(indices, (uint64)index_count * sizeof(uint32), RHI_USAGE_STATIC);
    m.index_count   = index_count;

    return m;
}

void mesh_draw(const mesh* m)
{
    rhi_draw_indexed(m->vertex_buffer, m->index_buffer, m->index_count);
}

void mesh_destroy(mesh* m)
{
    if (!m)
        return;

    rhi_buffer_destroy(m->vertex_buffer);
    rhi_buffer_destroy(m->index_buffer);

    m->vertex_buffer = NULL;
    m->index_buffer  = NULL;
    m->index_count   = 0;
}
