#version 330 core
in vec2 v_Uv;
uniform sampler2D u_Source;
uniform sampler2D u_Bloom;
uniform sampler2D u_SoftScene;
uniform bool u_Enabled;
uniform float u_Exposure;
uniform float u_BloomStrength;
uniform vec3 u_Vignette;
uniform vec3 u_EdgeBlur;
out vec4 FragColor;

vec3 filmic(vec3 color)
{
    return clamp((color * (2.51 * color + 0.03)) / (color * (2.43 * color + 0.59) + 0.14), 0.0, 1.0);
}

void main()
{
    vec3 color = texture(u_Source, v_Uv).rgb;
    if (u_Enabled)
    {
        // Normalize each screen axis: both portrait and landscape retain a clear center.
        float radius = length((v_Uv - 0.5) * 2.0);
        float softness = smoothstep(u_EdgeBlur.y, u_EdgeBlur.z, radius) * u_EdgeBlur.x;
        color = mix(color, texture(u_SoftScene, v_Uv).rgb, softness);
        color += texture(u_Bloom, v_Uv).rgb * u_BloomStrength;
        color = filmic(color * u_Exposure);
        float vignette = smoothstep(u_Vignette.y, u_Vignette.z, radius);
        color *= 1.0 - vignette * u_Vignette.x;
    }
    // The window is SDR; encode once here, then draw the display-referred UI.
    FragColor = vec4(pow(clamp(color, 0.0, 1.0), vec3(1.0 / 2.2)), 1.0);
}
