#version 330 core
in vec4 v_Color;
in vec2 v_Uv;
flat in int v_Textured;
flat in int v_Material;
in vec2 v_Local;
uniform sampler2D u_Texture;
uniform bool u_LinearOutput;
out vec4 FragColor;

float surfaceNoise(vec2 p)
{
    vec2 cell = floor(p), f = fract(p);
    f = f*f*(3.0-2.0*f);
    vec4 n = fract(sin(vec4(dot(cell,vec2(127.1,311.7)),
        dot(cell+vec2(1,0),vec2(127.1,311.7)),
        dot(cell+vec2(0,1),vec2(127.1,311.7)),
        dot(cell+vec2(1),vec2(127.1,311.7))))*43758.5453);
    return mix(mix(n.x,n.y,f.x),mix(n.z,n.w,f.x),f.y);
}

void main()
{
    FragColor = v_Color;
    if (v_Textured != 0)
    {
        FragColor *= texture(u_Texture, v_Uv);
    }
    if (v_Material > 0 && v_Textured == 0)
    {
        // Local coordinates keep surface detail attached to the cached mesh, not the screen.
        float footprint = max(length(dFdx(v_Local)), length(dFdy(v_Local)));
        float detail = 1.0-smoothstep(.7, 2.0, footprint);
        float grain = surfaceNoise(v_Local*1.6)-.5;
        float broad = surfaceNoise(v_Local*.19)-.5;
        float strength = v_Material == 1 ? .025 : .09;
        float pattern = broad + grain*detail*.55;
        if (v_Material == 3)
        {
            pattern = surfaceNoise(v_Local*vec2(.65,.055))-.5 + grain*detail*.25;
        }
        FragColor.rgb *= 1.0 + pattern*strength;
    }
    if (u_LinearOutput)
    {
        FragColor.rgb = pow(clamp(FragColor.rgb, 0.0, 1.0), vec3(2.2)) + max(FragColor.rgb - 1.0, 0.0);
    }
}
