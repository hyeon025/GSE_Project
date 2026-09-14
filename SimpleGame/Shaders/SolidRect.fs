#version 330 core
in vec4 v_Color;
in vec2 v_Uv;
uniform sampler2D u_Texture;
uniform bool u_Textured;
uniform bool u_LinearOutput;
out vec4 FragColor;

void main()
{
    FragColor = v_Color;
    if (u_Textured)
    {
        FragColor *= texture(u_Texture, v_Uv);
    }
    // Existing material colors are display-referred; values above one are emission.
    if (u_LinearOutput)
    {
        FragColor.rgb = pow(clamp(FragColor.rgb, 0.0, 1.0), vec3(2.2)) + max(FragColor.rgb - 1.0, 0.0);
    }
}
