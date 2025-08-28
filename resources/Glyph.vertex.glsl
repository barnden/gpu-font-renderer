#version 450

uniform mat4 u_Projection;
uniform mat4 u_ModelView;

layout(std430, binding = 2) readonly buffer ssbo_indices
{
    uint b_Indices[];
};

in vec3 i_Position;
in vec2 i_TexCoord;
in int i_Glyph;

out vec2 v_TexCoord;
out float v_PixelsPerEm;
flat out uvec2 v_Contours;

void main()
{
    gl_Position = u_Projection * u_ModelView * vec4(i_Position, 1.0);
    v_TexCoord = i_TexCoord;

    uint glyph_start = b_Indices[i_Glyph];
    uint glyph_end = b_Indices[i_Glyph + 1];

    v_Contours = uvec2(glyph_start, glyph_end - glyph_start);
    v_PixelsPerEm = clamp(64.0 / gl_Position.w, 32.0, 2048.0);
}
