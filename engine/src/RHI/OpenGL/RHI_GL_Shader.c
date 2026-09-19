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

void gl_shader_set_mat4(rhi_shader shader, const char* name, const f32* matrix)
{
    GLint location = glGetUniformLocation(shader->program, name);
    if (location < 0)
        return; // unused-uniform-optimized-out or a typo'd name; not fatal

    // glProgramUniform (DSA, core since GL 4.1) writes to the given
    // program's uniform directly — unlike glUniform*, it does not require
    // that program to be the currently bound one via glUseProgram.
    glProgramUniformMatrix4fv(shader->program, location, 1, GL_FALSE, matrix);
}

void gl_shader_set_vec3(rhi_shader shader, const char* name, f32 x, f32 y, f32 z)
{
    GLint location = glGetUniformLocation(shader->program, name);
    if (location < 0)
        return;

    glProgramUniform3f(shader->program, location, x, y, z);
}

void gl_shader_set_int(rhi_shader shader, const char* name, int32 value)
{
    GLint location = glGetUniformLocation(shader->program, name);
    if (location < 0)
        return;

    glProgramUniform1i(shader->program, location, value);
}
