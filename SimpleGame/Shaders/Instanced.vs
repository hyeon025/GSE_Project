#version 330 core
layout(location = 0) in vec4 a_Matrix;
layout(location = 1) in vec4 a_Placement;
layout(location = 2) in vec4 a_ColorA;
layout(location = 3) in vec4 a_ColorB;
layout(location = 4) in vec4 a_ColorC;
layout(location = 5) in vec4 a_Options;
layout(location = 6) in vec4 a_UvRect;
layout(location = 7) in vec4 a_Skin;
layout(location = 8) in vec4 a_Hair;
layout(location = 9) in vec4 a_Cloth;
layout(location = 10) in vec4 a_Trim;
layout(location = 11) in vec4 a_Depth;
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
    float localDepth = a_Depth.z > .5 ? clamp(second.z, 0.0, 1.0) : .5;
    float depth = a_Depth.x - a_Depth.y * (.1 + .8 * localDepth);
    gl_Position = vec4(position.x / u_Viewport.x * 2.0 - 1.0,
        1.0 - position.y / u_Viewport.y * 2.0, depth * 2.0 - 1.0, 1);
    v_Color = a_Options.x < 0.5 ? color :
        color.r * a_ColorA + color.g * a_ColorB + color.b * a_ColorC;
    if(a_Options.w > .5 && color.r < -.5)
    {
        // Negative red stores a palette role, green stores the baked face light.
        // One pose/mesh serves every NPC color combination in the same draw batch.
        int role=int(-color.r+.5);
        vec3 base=a_Skin.rgb;
        if(role==2)base*=vec3(.76,.55,.52);
        else if(role==3)base=mix(base,vec3(.55,.14,.10),.24);
        else if(role==4)base=a_Hair.rgb;
        else if(role==5)base=min(vec3(1),a_Hair.rgb*1.3+vec3(.012));
        else if(role==6)base=a_Cloth.rgb;
        else if(role==7)base=a_Trim.rgb;
        else if(role==8 || role==9)
        {
            base=mix(vec3(.65,.57,.40),a_Cloth.rgb,.12);
            if(role==9)base*=.70;
        }
        v_Color=vec4(pow(max(vec3(0),base*color.g),vec3(1.0/2.2)),color.a);
    }
    v_Color.a *= a_Options.y;
    v_Uv = a_UvRect.xy + first.xy * a_UvRect.zw;
}
