#version 430 core

out vec4 FragColor;

uniform sampler2D gPosition;
uniform sampler2D texNoise;
uniform vec3 samples[64];
uniform mat4 projection;
uniform mat4 view;
uniform vec2 iResolution;
uniform int kernelSize;
uniform float radius;
uniform float bias;

void main()
{
    vec2 texCoords = gl_FragCoord.xy / iResolution;

    vec3 fragPosWorld = texture(gPosition, texCoords).xyz;
    vec3 fragPosView = vec3(view * vec4(fragPosWorld, 1.0));

    // reconstruct normal in view-space from position derivatives
    vec3 normal = normalize(cross(dFdx(fragPosView), dFdy(fragPosView)));
    if (length(normal) < 1e-6) {
        normal = vec3(0.0, 0.0, 1.0);
    }

    vec3 randomVec = texture(texNoise, texCoords * (iResolution / 4.0)).xyz;

    vec3 tangent = normalize(randomVec - normal * dot(randomVec, normal));
    vec3 bitangent = cross(normal, tangent);
    mat3 TBN = mat3(tangent, bitangent, normal);

    float occlusion = 0.0;
    for (int i = 0; i < kernelSize; ++i) {
        vec3 sampleView = fragPosView + TBN * samples[i] * radius;

        vec4 offset = projection * vec4(sampleView, 1.0);
        offset.xyz /= offset.w;
        vec2 sampleUV = offset.xy * 0.5 + 0.5;

        if (sampleUV.x < 0.0 || sampleUV.y < 0.0 || sampleUV.x > 1.0 || sampleUV.y > 1.0)
            continue;

        vec3 sampleWorld = texture(gPosition, sampleUV).xyz;
        vec3 sampleViewFromTex = vec3(view * vec4(sampleWorld, 1.0));

        float rangeCheck = smoothstep(0.0, 1.0, radius / max(0.0001, length(fragPosView - sampleViewFromTex)));
        if (sampleViewFromTex.z >= sampleView.z + bias)
            occlusion += rangeCheck;
    }

    occlusion = 1.0 - (occlusion / float(kernelSize));
    FragColor = vec4(vec3(occlusion), 1.0);
}
// Left empty. This is optional. For implemetation, you can find many references from nearby shaders. You might need random number generators (RNG) to distribute points in a (Hemi)sphere. You can ask AI for both of them (RNG and sampling in a sphere) or try to find some resources online. Later I will add some links to the document about this. 