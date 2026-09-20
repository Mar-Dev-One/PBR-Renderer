# PBR-Renderer
A modern OpenGL PBR renderer implementing physically based shading, HDR rendering, image-based lighting (IBL), shadow mapping, and a modular graphics architecture (Uncompleted yet).

## Model loading

`model_load(path)` (`engine/src/Scene/Model.h`) loads by file extension:

| Format | Loader | Notes |
|---|---|---|
| `.gltf` / `.glb` | [cgltf](https://github.com/jkuhlmann/cgltf) | Scene hierarchy, multiple meshes/materials, embedded or external textures, PBR metallic-roughness (base color, metal/rough, normal, occlusion, emissive), alpha mask/blend, double-sided. |
| `.obj` (+ `.mtl`) | [fast_obj](https://github.com/thisistherk/fast_obj) | Vertices are welded, `Kd`/`Ke`/`Ns`/`d` and `map_Kd`/`map_Ke` become a PBR material. |

Every format is converted to the same `model` / `material` / `mesh` types and drawn with
`model_draw()` and `assets/shaders/pbr.*`. Not supported yet: skinning/animation, morph targets,
Draco/meshopt compression, `KHR_texture_transform`, and second UV sets.

Drop a file in `assets/models/` and add it to `MODEL_PATHS` in `testbed/main.c` to view it.
