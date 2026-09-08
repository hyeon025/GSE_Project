#version 330 core
layout(location=0) in vec2 a_Position;
layout(location=1) in vec4 a_Color;
layout(location=2) in vec2 a_Uv;
uniform vec2 u_Viewport;
out vec4 v_Color;
out vec2 v_Uv;
void main() {
    gl_Position=vec4(a_Position.x/u_Viewport.x*2.0-1.0,1.0-a_Position.y/u_Viewport.y*2.0,0,1);
    v_Color=a_Color; v_Uv=a_Uv;
}
