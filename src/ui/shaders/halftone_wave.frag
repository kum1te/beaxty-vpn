#version 440

layout(location = 0) in vec2 qt_TexCoord0;
layout(location = 0) out vec4 fragColor;

layout(std140, binding = 0) uniform buf {
    mat4 qt_Matrix;
    float qt_Opacity;
    float u_time;
    vec2 u_resolution;
    vec4 u_bgColor;
    vec4 u_dotColor;
    vec4 u_accentColor;
};

void main() {
    // Pixel coordinates
    vec2 uv = qt_TexCoord0 * u_resolution;

    // Halftone grid sizing: 16px cells
    float cellSize = 16.0;
    vec2 gridId = floor(uv / cellSize);
    vec2 cellUv = fract(uv / cellSize) - vec2(0.5); // centered in [-0.5, 0.5]

    // Normalized grid position [0..1]
    vec2 normPos = (gridId * cellSize) / max(u_resolution, vec2(1.0));

    // Multi-frequency sinusoidal interference wave matching bganim.mp4
    float w1 = sin(normPos.x * 6.28318 * 2.2 + normPos.y * 6.28318 * 1.4 + u_time * 1.15);
    float w2 = cos(normPos.x * 6.28318 * 1.6 - normPos.y * 6.28318 * 2.3 + u_time * 0.85);
    float w3 = sin(length(normPos - vec2(0.48, 0.52)) * 9.0 - u_time * 1.4);
    float w4 = cos((normPos.x - normPos.y) * 4.0 + u_time * 0.6);

    float wave = (w1 + w2 + w3 + w4) * 0.25; // in [-1, 1]
    float waveNorm = clamp(0.5 + 0.5 * wave, 0.0, 1.0);

    // Dynamic dot radius based on wave height
    float minRadius = 0.07;
    float maxRadius = 0.44;
    float radius = mix(minRadius, maxRadius, pow(waveNorm, 1.35));

    // Antialiased circular dot with smooth edge
    float dist = length(cellUv);
    float dotAlpha = 1.0 - smoothstep(radius - 0.04, radius + 0.02, dist);

    // Soft radial vignette to keep edges deep and content readable
    vec2 centerOffset = qt_TexCoord0 - vec2(0.5);
    float vignette = clamp(1.0 - dot(centerOffset, centerOffset) * 1.6, 0.0, 1.0);

    // Color mixing: background -> dot color -> accent crest glow
    vec4 color = mix(u_bgColor, u_dotColor, dotAlpha * vignette * 0.85);
    color += u_accentColor * (dotAlpha * vignette * pow(waveNorm, 2.2) * 0.45);

    fragColor = color * qt_Opacity;
}
