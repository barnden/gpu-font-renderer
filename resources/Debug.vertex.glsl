#version 450

uniform mat4 u_Projection;
uniform mat4 u_ModelView;

in vec3 i_Position;
in vec2 i_TexCoord;

out vec2 v_TexCoord;

void main() {
    gl_Position = u_Projection * u_ModelView * vec4(i_Position, 1.0);
    v_TexCoord = i_TexCoord;
}