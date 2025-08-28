#version 450

in vec2 v_TexCoord;

out vec4 o_Color;

void main() {
    vec3 color = vec3(0.5, 0.1, 0.1);

    float t = floor(v_TexCoord.x * 16.0) + floor(v_TexCoord.y * 16.0);
    if (mod(t, 2.0) == 0)
        color = vec3(.125);

    o_Color = vec4(color, .875);
}