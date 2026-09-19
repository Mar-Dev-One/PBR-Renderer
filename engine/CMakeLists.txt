add_library(PBR_Engine STATIC
    src/App.c
    src/Core/FileIO.c
    src/Core/Paths.c
    src/Core/Image.c
    src/Renderer/Renderer.c
    src/RHI/RHI.c
    src/RHI/OpenGL/RHI_GL.c
    src/RHI/OpenGL/RHI_GL_Buffer.c
    src/RHI/OpenGL/RHI_GL_Shader.c
    src/RHI/OpenGL/RHI_GL_Texture.c
    src/RHI/OpenGL/RHI_GL_Framebuffer.c
    src/Platform/Window.c
    src/Scene/Camera.c
    src/UI/ImGuiLayer.c
)

# PUBLIC: consumers (testbed) #include headers like "App.h", "RHI/RHI.h",
# "Core/Paths.h" straight from this root, same paths as before the split.
# NOTE: this also exposes RHI/OpenGL/RHI_GL_Internal.h, which is meant to
# stay backend-private -- that's a real header/include boundary this pass
# does not enforce (would need a dedicated include/PBR/ split), only
# documents via the comment already in that file.
target_include_directories(PBR_Engine
    PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/src
)

add_subdirectory(deps/glfw)
add_subdirectory(deps/glad)
add_subdirectory(deps/cglm)
include(cmake/cimgui.cmake)

# PUBLIC: Scene/Camera.h includes <cglm/cglm.h> directly, and testbed calls
# cimgui's ig*() functions directly, so both must be visible transitively.
# glfw/glad are PUBLIC too, defensively, even though Platform/Window.h only
# forward-declares GLFWwindow as an opaque type today.
target_link_libraries(PBR_Engine PUBLIC
    glfw
    glad
    OpenGL::GL
    cglm
    cimgui
)

# stb_image is a single vendored header (no CMakeLists of its own, unlike
# the git-submodule deps above), needed only by Core/Image.c -- PRIVATE.
target_include_directories(PBR_Engine PRIVATE deps/stb)

# PRIVATE: only Core/Paths.c reads this macro (asset_path() bakes it in at
# runtime). testbed/main.c never references ASSETS_ROOT itself, so there's
# no need to leak this build-config detail -- or force it -- onto every
# future consumer of PBR_Engine.
target_compile_definitions(PBR_Engine PRIVATE
    ASSETS_ROOT="${CMAKE_SOURCE_DIR}/assets"
)
