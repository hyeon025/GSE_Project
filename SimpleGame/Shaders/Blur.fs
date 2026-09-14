#version 330 core
in vec2 v_Uv;
uniform sampler2D u_Source;
uniform vec2 u_Direction;
out vec4 FragColor;

void main()
{
    vec3 color = texture(u_Source, v_Uv).rgb * 0.227027;
    color += texture(u_Source, v_Uv + u_Direction * 1.384615).rgb * 0.316216;
    color += texture(u_Source, v_Uv - u_Direction * 1.384615).rgb * 0.316216;
    color += texture(u_Source, v_Uv + u_Direction * 3.230769).rgb * 0.0702705;
    color += texture(u_Source, v_Uv - u_Direction * 3.230769).rgb * 0.0702705;
    FragColor = vec4(color, 1.0);
}
