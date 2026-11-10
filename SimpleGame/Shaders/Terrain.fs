#version 330 core
uniform vec2 u_Viewport;
uniform vec2 u_Camera;
uniform float u_Zoom;
uniform vec2 u_View;
uniform vec4 u_GroundFromScreen;
uniform bool u_LinearOutput;
uniform bool u_Farming;
uniform float u_Seed;
uniform vec2 u_Villages[7];
uniform float u_VillageRadius;
uniform float u_HouseSpacing;
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
    float distance = 1e10;
    for (int i = 0; i < 7; ++i)
    {
        vec2 q = p - u_Villages[i];
        if (length(q) < u_VillageRadius)
        {
            distance = min(distance, min(abs(q.x),abs(q.y)));
            vec2 lane = abs(mod(q + u_HouseSpacing*.5,u_HouseSpacing)-u_HouseSpacing*.5);
            distance = min(distance,min(lane.x,lane.y));
        }
        for (int j = i + 1; j < 7; ++j)
        {
            vec2 segment = u_Villages[j]-u_Villages[i];
            if (length(segment) > 1717.0) continue;
            float t = clamp(dot(q,segment)/dot(segment,segment),0.0,1.0);
            distance = min(distance,length(q-segment*t));
        }
    }
    return min(distance, min(
        abs(p.x - p.y) * 0.7071,
        min(abs(p.x + p.y + 128.0) * 0.7071, abs(p.y - sin(p.x / 145.0) * 100.0 + 320.0) * 0.78)
    ));
}

void main()
{
    vec2 screen = vec2(gl_FragCoord.x, u_Viewport.y - gl_FragCoord.y);
    vec2 d = (screen - vec2(u_Viewport.x * 0.5, u_Viewport.y * u_View.y)) / u_Zoom;
    vec2 p = u_Camera + u_GroundFromScreen.xy * d.x + u_GroundFromScreen.zw * d.y;
    float broad = noise(p * 0.008), fine = noise(p * 0.23), grain = noise(p * 1.25);
    float dirt = smoothstep(24.0, 43.0, road(p) + noise(p * 0.06) * 8.0);
    float meadow = noise(p * 0.026 + vec2(31.0, 17.0));
    vec3 grass = mix(vec3(0.10, 0.275, 0.095), vec3(0.265, 0.445, 0.15), broad * 0.65 + meadow * 0.35);
    grass += (fine - 0.5) * vec3(0.012, 0.025, 0.009);
    vec3 path = mix(vec3(0.31, 0.295, 0.26), vec3(0.45, 0.42, 0.355), fine * 0.55 + broad * 0.45);
    float detailFade = 1.0 - smoothstep(0.6, 2.0, max(fwidth(p.x), fwidth(p.y)));
    vec3 color = mix(path, grass, dirt) + (fine - 0.5) * mix(0.04, 0.012, dirt);
    color += (grain - 0.5) * 0.012 * detailFade;
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
    float stoneRadius = length(local * vec2(1.0, 1.5));
    float stone = (1.0 - smoothstep(0.10, 0.24, stoneRadius)) * step(0.76, hash(cell));
    float exposedStone = stone * (1.0-dirt*.96);
    vec3 stoneColor = mix(vec3(.30,.33,.33), vec3(.49,.50,.47), hash(cell+17.0));
    color = mix(color, stoneColor, exposedStone);
    float damp = smoothstep(0.66, 0.83, noise(p * 0.018)) * smoothstep(0.45, 0.65, broad);
    vec3 dampColor = mix(vec3(0.18, 0.22, 0.19), vec3(0.095, 0.245, 0.11), dirt);
    color = mix(color, dampColor, damp * 0.28);
    // Shading-only micro-relief: navigation and collision still use the existing flat map.
    float relief = fine*.35 + noise(p*.065)*.9 + exposedStone*1.25;
    if (u_Farming)
    {
        relief += sin((p.x+p.y*.12)*.24)*dirt*.55;
    }
    vec3 surface = vec3(p, relief);
    vec3 normal = normalize(cross(dFdx(surface), dFdy(surface)));
    if (normal.z < 0.0) normal = -normal;
    vec3 light = normalize(vec3(-.45,.55,.85));
    vec3 view = normalize(vec3(-2.0 * u_View.x * u_GroundFromScreen.zw, 1.5));
    float diffuse = max(dot(normal,light),0.0);
    float cavity = 1.0 - (1.0-fine)*.08 - (1.0-stone)*exposedStone*.12;
    color *= (.40+.72*diffuse)*cavity;
    float wet = damp*(1.0-dirt*.7);
    float specular = pow(max(dot(normal,normalize(light+view)),0.0),24.0)*wet*.075;
    color += vec3(.66,.73,.75)*specular;
    if (u_LinearOutput)
    {
        color = pow(max(color, 0.0), vec3(2.2));
    }
    FragColor = vec4(color, 1);
}
