#version 330 core
uniform vec2 u_Viewport;
uniform vec2 u_Camera;
uniform float u_Zoom;
uniform bool u_LinearOutput;
uniform bool u_Farming;
uniform float u_Seed;
out vec4 FragColor;

float hash(vec2 p)
{
    return fract(sin(dot(p, vec2(127.1, 311.7))) * 43758.5453);
}

float noise(vec2 p)
{
    vec2 i = floor(p), f = fract(p);
    f = f * f * (3.0 - 2.0 * f);
    return mix(mix(hash(i), hash(i + vec2(1, 0)), f.x), mix(hash(i + vec2(0, 1)), hash(i + 1.0), f.x), f.y);
}

float road(vec2 p)
{
    if (u_Farming)
    {
        return min(min(abs(p.x), abs(p.y)), abs(max(abs(p.x), abs(p.y)) - 432.0));
    }
    return min(
        abs(p.x - p.y) * 0.7071,
        min(abs(p.x + p.y + 128.0) * 0.7071, abs(p.y - sin(p.x / 145.0) * 100.0 + 320.0) * 0.78)
    );
}

void main()
{
    vec2 screen = vec2(gl_FragCoord.x, u_Viewport.y - gl_FragCoord.y);
    vec2 d = (screen - vec2(u_Viewport.x * 0.5, u_Viewport.y * 0.55)) / u_Zoom;
    vec2 p = u_Camera + vec2(d.x * 0.5 - d.y, -d.x * 0.5 - d.y);
    float broad = noise(p * 0.008), fine = noise(p * 0.23), grain = noise(p * 1.25);
    float dirt = smoothstep(24.0, 43.0, road(p) + noise(p * 0.06) * 8.0);
    vec3 grass = mix(vec3(0.16, 0.205, 0.17), vec3(0.28, 0.30, 0.235), broad);
    vec3 path = mix(vec3(0.31, 0.295, 0.26), vec3(0.45, 0.42, 0.355), fine * 0.55 + broad * 0.45);
    vec3 color = mix(path, grass, dirt) + (fine - 0.5) * 0.055 + (grain - 0.5) * 0.035;
    if (u_Farming)
    {
        float furrow = smoothstep(0.15, 0.55, abs(sin((p.x + p.y * 0.12) * 0.24)));
        vec3 soil = mix(vec3(0.22, 0.235, 0.19), vec3(0.32, 0.32, 0.25), furrow);
        color = mix(color, soil, dirt * 0.48);
        color += (noise(p * 0.04 + u_Seed) - 0.5) * 0.035;
        if (max(abs(p.x), abs(p.y)) > 744.0)
        {
            color *= 0.32;
        }
    }
    vec2 cell = floor(p / 9.0), local = fract(p / 9.0) - 0.5;
    float stone = (1.0 - smoothstep(0.10, 0.24, length(local * vec2(1.0, 1.5)))) * step(0.76, hash(cell));
    color = mix(color, vec3(0.46, 0.46, 0.42) * (0.85 + local.y * 0.3), stone * (1.0 - dirt * 0.8));
    float damp = smoothstep(0.66, 0.83, noise(p * 0.018)) * smoothstep(0.45, 0.65, broad);
    color = mix(color, vec3(0.16, 0.21, 0.205), damp * 0.55);
    if (u_LinearOutput)
    {
        color = pow(max(color, 0.0), vec3(2.2));
    }
    FragColor = vec4(color, 1);
}
