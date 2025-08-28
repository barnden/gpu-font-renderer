#version 450

layout(std430, binding = 0) readonly buffer ssbo_points
{
    vec2 b_Points[];
};

layout(std430, binding = 1) readonly buffer ssbo_contours
{
    uint b_Contours[];
};

in vec2 v_TexCoord;
in float v_PixelsPerEm;
flat in uvec2 v_Contours;

out vec4 o_FragColor;

vec2 rotate(vec2 v, float angle)
{
    float c = cos(angle);
    float s = sin(angle);

    return vec2(v.x * c - v.y * s, v.x * s + v.y * c);
}

vec2 interpolate(in float t,
                 in vec2 p1,
                 in vec2 p2,
                 in vec2 p3)
{
    return (1 - t) * (1 - t) * p1 + 2 * t * (1 - t) * p2 + t * t * p3;
}

void get_contribution(inout float alpha,
                      in vec2 p1,
                      in vec2 p2,
                      in vec2 p3)
{
    int shift = 2 * int(p1.t > 0) + 4 * int(p2.t > 0) + 8 * int(p3.t > 0);
    int result = 0x2E74 >> shift;

    if ((result & 3) == 0)
        return;

    float a = p1.t - 2.0 * p2.t + p3.t;
    float b = p1.t - p2.t;
    float c = p1.t;

    float t1 = 0.0;
    float t2 = 0.0;

    if (abs(a) < 1e-4) {
        t1 = c / (2.0 * b);
        t2 = c / (2.0 * b);
    } else {
        float d = sqrt(max(b * b - a * c, 0.0));
        t1 = (b - d) / a;
        t2 = (b + d) / a;
    }

    if ((result & 1) > 0) {
        alpha += clamp(v_PixelsPerEm * interpolate(t1, p1, p2, p3).s + 0.5, 0.0, 1.0);
    }

    if ((result & 2) > 0) {
        alpha -= clamp(v_PixelsPerEm * interpolate(t2, p1, p2, p3).s + 0.5, 0.0, 1.0);
    }
}

const float PI = 3.14159265359;
const int num_directions = 4;
void main()
{
    uint glyph_start = v_Contours.x;
    uint num_contours = v_Contours.y;

    float alpha = 0.0;
    for (uint i = 0; i < num_contours; i++) {
        uint idx = glyph_start + i;

        uint contour_start = b_Contours[idx];
        uint contour_end = b_Contours[idx + 1];
        uint num_points = contour_end - contour_start;

        for (int j = 0; j < num_points; j += 2) {
            vec2 p1 = b_Points[contour_start + j] - v_TexCoord;
            vec2 p2 = b_Points[contour_start + ((j + 1) % num_points)] - v_TexCoord;
            vec2 p3 = b_Points[contour_start + ((j + 2) % num_points)] - v_TexCoord;

            for (int k = 0; k <= num_directions; k++) {
                get_contribution(alpha, 
                                 rotate(p1, float(k) * PI / float(num_directions)), 
                                 rotate(p2, float(k) * PI / float(num_directions)), 
                                 rotate(p3, float(k) * PI / float(num_directions)));
            }
        }
    }

    alpha = clamp(alpha, 0.0, 1.0);

    o_FragColor = vec4(vec3(0.), alpha);
}
