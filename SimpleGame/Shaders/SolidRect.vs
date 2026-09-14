#version 330 core
layout(location = 0) in vec2 a_Position;
layout(location = 1) in vec4 a_Color;
layout(location = 2) in vec2 a_Uv;
uniform vec2 u_Viewport;
uniform vec3 u_OffsetScale;
uniform float u_Opacity;
out vec4 v_Color;
out vec2 v_Uv;

void main()
{
    vec2 position = a_Position * u_OffsetScale.z + u_OffsetScale.xy;
    gl_Position = vec4(position.x / u_Viewport.x * 2.0 - 1.0, 1.0 - position.y / u_Viewport.y * 2.0, 0, 1);
    v_Color = vec4(a_Color.rgb, a_Color.a * u_Opacity);
    v_Uv = a_Uv;
}
