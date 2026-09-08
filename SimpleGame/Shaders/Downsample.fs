#version 330 core
in vec2 v_Uv;
uniform sampler2D u_Source;
uniform bool u_Highlights;
uniform float u_Threshold;
out vec4 FragColor;
vec3 sampleColor(vec2 uv)
{
    vec3 color = texture(u_Source, uv).rgb;
    if (!u_Highlights) return color;
    float luminance = dot(color, vec3(0.2126, 0.7152, 0.0722));
    float knee = max(u_Threshold * 0.5, 0.001);
    float soft = clamp(luminance - u_Threshold + knee, 0.0, 2.0 * knee);
    soft = soft * soft / (4.0 * knee);
    float contribution = max(luminance - u_Threshold, soft) / max(luminance, 0.0001);
    return color * contribution;
}
void main()
{
    vec2 stepSize = 0.5 / vec2(textureSize(u_Source, 0));
    // Extract before averaging so small emissive features survive downsampling.
    vec3 color = sampleColor(v_Uv + stepSize * vec2(-1, -1));
    color += sampleColor(v_Uv + stepSize * vec2(1, -1));
    color += sampleColor(v_Uv + stepSize * vec2(-1, 1));
    color += sampleColor(v_Uv + stepSize * vec2(1, 1));
    FragColor = vec4(color * 0.25, 1.0);
}
