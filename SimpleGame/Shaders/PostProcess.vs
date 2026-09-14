#version 330 core
out vec2 v_Uv;

void main()
{
    // A fullscreen triangle avoids a diagonal interpolation seam.
    vec2 p = vec2((gl_VertexID << 1) & 2, gl_VertexID & 2);
    v_Uv = p;
    gl_Position = vec4(p * 2.0 - 1.0, 0.0, 1.0);
}
