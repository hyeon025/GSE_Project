#version 330 core
layout(location = 0) in vec4 a_Matrix;
layout(location = 1) in vec4 a_Placement;
layout(location = 2) in vec4 a_ColorA;
layout(location = 3) in vec4 a_ColorB;
layout(location = 4) in vec4 a_ColorC;
layout(location = 5) in vec4 a_Options;
layout(location = 6) in vec4 a_UvRect;
uniform samplerBuffer u_Meshes;
uniform vec2 u_Viewport;
out vec4 v_Color;
out vec2 v_Uv;
flat out int v_Textured;
flat out int v_Material;
out vec2 v_Local;

void main()
{
    v_Textured = int(a_Options.x + 0.5) == 2 ? 1 : 0;
    v_Material = int(a_Options.z + 0.5);
    v_Local = vec2(0);
    v_Color = vec4(0);
    v_Uv = vec2(0);
    if (gl_VertexID >= int(a_Placement.w + 0.5))
    {
        // Mesh counts are multiples of three: padding produces only degenerate triangles.
        gl_Position = vec4(2, 2, 0, 1);
        return;
    }
    int at = (int(a_Placement.z + 0.5) + gl_VertexID) * 2;
    vec4 first = texelFetch(u_Meshes, at);
    vec4 second = texelFetch(u_Meshes, at + 1);
    vec4 color = vec4(first.zw, second.xy);
    v_Local = first.xy;
    vec2 position = vec2(
        first.x * a_Matrix.x + first.y * a_Matrix.z,
        first.x * a_Matrix.y + first.y * a_Matrix.w) + a_Placement.xy;
    gl_Position = vec4(position.x / u_Viewport.x * 2.0 - 1.0,
        1.0 - position.y / u_Viewport.y * 2.0, 0, 1);
    v_Color = a_Options.x < 0.5 ? color :
        color.r * a_ColorA + color.g * a_ColorB + color.b * a_ColorC;
    v_Color.a *= a_Options.y;
    v_Uv = a_UvRect.xy + first.xy * a_UvRect.zw;
}
