#version 430 core

out vec4 FragColor;

uniform sampler2D ssaoInput;
uniform vec2 iResolution;

void main()
{
    vec2 uv = gl_FragCoord.xy / iResolution;
    vec2 texel = 1.0 / iResolution;

    float result = 0.0;
    for (int x = -1; x <= 1; ++x) {
        for (int y = -1; y <= 1; ++y) {
            result += texture(ssaoInput, uv + vec2(x, y) * texel).r;
        }
    }
    result /= 9.0;
    FragColor = vec4(result, result, result, 1.0);
}
