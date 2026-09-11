#include "RHI_GL_Internal.h"

static GLuint
compile_stage(GLenum stage, const char* src)
{
    GLuint shader = glCreateShader(stage);
    glShaderSource(shader, 1, &src, NULL);
    glCompileShader(shader);

    GLint success = 0;
    glGetShaderiv(shader, GL_COMPILE_STATUS, &success);

    if (!success)
    {
        char log[512];
        glGetShaderInfoLog(shader, sizeof(log), NULL, log);
        LOG_ERROR("Shader compile error: %s\n", log);
        glDeleteShader(shader);
        return 0;
    }

    return shader;
}

rhi_shader gl_shader_create(rhi_shader_desc desc)
{
    GLuint vs = compile_stage(GL_VERTEX_SHADER, desc.vertex_src);
    GLuint fs = compile_stage(GL_FRAGMENT_SHADER, desc.fragment_src);

    if (!vs || !fs)
    {
        if (vs) glDeleteShader(vs);
        if (fs) glDeleteShader(fs);
        return NULL;
    }

    GLuint program = glCreateProgram();
    glAttachShader(program, vs);
    glAttachShader(program, fs);
    glLinkProgram(program);

    GLint success = 0;
    glGetProgramiv(program, GL_LINK_STATUS, &success);

    glDeleteShader(vs);
    glDeleteShader(fs);

    if (!success)
    {
        char log[512];
        glGetProgramInfoLog(program, sizeof(log), NULL, log);
        LOG_ERROR("Shader link error: %s\n", log);
        glDeleteProgram(program);
        return NULL;
    }

    rhi_shader shader = malloc(sizeof(*shader));
    if (!shader)
        FATAL("Out of memory creating shader");

    shader->program = program;
    return shader;
}

void gl_shader_bind(rhi_shader shader)
{
    glUseProgram(shader->program);
}

void gl_shader_destroy(rhi_shader shader)
{
    if (!shader)
        return;

    glDeleteProgram(shader->program);
    free(shader);
}
