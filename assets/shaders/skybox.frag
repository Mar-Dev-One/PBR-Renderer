#version 460 core

// Environment background. Goes through the same exposure + ACES + gamma
// chain as pbr.frag so the sky and the lit model share one look.

in vec3 v_dir;

out vec4 frag_color;

uniform samplerCube u_environment;
uniform float u_exposure;
uniform float u_lod;   // mip level to read: 0 = sharp, higher = blurrier

vec3 aces_tonemap(vec3 x)
{
    return clamp((x * (2.51 * x + 0.03)) / (x * (2.43 * x + 0.59) + 0.14), 0.0, 1.0);
}

void main()
{
    vec3 color = textureLod(u_environment, normalize(v_dir), u_lod).rgb;

    color = aces_tonemap(color * u_exposure);
    color = pow(color, vec3(1.0 / 2.2));

    frag_color = vec4(color, 1.0);
}
