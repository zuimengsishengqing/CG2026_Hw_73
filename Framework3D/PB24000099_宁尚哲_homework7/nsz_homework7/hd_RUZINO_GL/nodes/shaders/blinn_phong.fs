#version 430 core

// Define a uniform struct for lights
struct Light {
    // The matrices are used for shadow mapping. You need to fill it according to how we are filling it when building the normal maps (node_render_shadow_mapping.cpp). 
    // Now, they are filled with identity matrix. You need to modify C++ code innode_render_deferred_lighting.cpp.
    // Position and color are filled.
    mat4 light_projection;
    mat4 light_view;
    vec3 position;
    float radius;
    vec3 color; // Just use the same diffuse and specular color.
    int shadow_map_id;
};
 
layout(binding = 0) buffer lightsBuffer {
Light lights[4];
};

uniform vec2 iResolution;

uniform sampler2D diffuseColorSampler;
uniform sampler2D normalMapSampler; // You should apply normal mapping in rasterize_impl.fs
uniform sampler2D metallicRoughnessSampler;
uniform sampler2DArray shadow_maps;
uniform sampler2D position;

// uniform float alpha;
uniform vec3 camPos;

uniform int light_count;

layout(location = 0) out vec4 Color;

// ----------------------- PCSS soft shadow helpers -----------------------
const int PCSS_BLOCKER_SAMPLES = 16;
const int PCSS_PCF_SAMPLES = 16;
const float SHADOW_BIAS = 0.005;
const float PCSS_MIN_RADIUS_UV = 0.0005; // clamp radius (uv units)
const float PCSS_MAX_RADIUS_UV = 0.05;   // clamp radius (uv units)

// Poisson disk samples (16 samples)
const vec2 poissonDisk[16] = vec2[](
    vec2(-0.94201624, -0.39906216),
    vec2(0.94558609, -0.76890725),
    vec2(-0.094184101, -0.92938870),
    vec2(0.34495938, 0.29387760),
    vec2(-0.91588581, 0.45771432),
    vec2(-0.81544232, -0.87912464),
    vec2(-0.38277543, 0.27676845),
    vec2(0.97484398, 0.75648379),
    vec2(0.44323325, -0.97511554),
    vec2(0.53742981, -0.47373420),
    vec2(-0.26496911, -0.41893023),
    vec2(0.79197514, 0.19090188),
    vec2(-0.24188840, 0.99706507),
    vec2(-0.81409955, 0.91437590),
    vec2(0.19984126, 0.78641367),
    vec2(0.14383161, -0.14100790)
);

// Find average depth of blockers within a small search region.
// Returns -1.0 if no blockers are found.
float findBlockerAvgDepth(sampler2DArray sm, vec2 uv, float receiverDepth, int layer, float searchRadiusUV) {
    int blockers = 0;
    float avg = 0.0;
    for (int i = 0; i < PCSS_BLOCKER_SAMPLES; ++i) {
        vec2 offset = poissonDisk[i] * searchRadiusUV;
        float d = texture(sm, vec3(uv + offset, layer)).r;
        if (d + SHADOW_BIAS < receiverDepth) {
            avg += d;
            blockers++;
        }
    }
    if (blockers == 0) return -1.0;
    return avg / float(blockers);
}

// PCF filter: simple percentage-closer filtering with Poisson samples
float pcfFilter(sampler2DArray sm, vec2 uv, float receiverDepth, int layer, float radiusUV) {
    float sum = 0.0;
    for (int i = 0; i < PCSS_PCF_SAMPLES; ++i) {
        vec2 offset = poissonDisk[i] * radiusUV;
        float d = texture(sm, vec3(uv + offset, layer)).r;
        sum += (d + SHADOW_BIAS < receiverDepth) ? 0.0 : 1.0;
    }
    return sum / float(PCSS_PCF_SAMPLES);
}

// Main PCSS routine. Uses blocker search then adaptive PCF radius.
float computePCSS(sampler2DArray sm, vec2 uv, float receiverDepth, int layer, float lightRadius) {
    // small search radius for blockers (in UV space) - can be tuned
    float blockerSearchRadius = 0.01; // ~1% of shadow map by default

    // 1) Blocker search
    float avgBlocker = findBlockerAvgDepth(sm, uv, receiverDepth, layer, blockerSearchRadius);
    if (avgBlocker < 0.0) {
        // no blocker found -> fully lit
        return 1.0;
    }

    // 2) Estimate penumbra size (UV units)
    // penumbra roughly proportional to (receiverDepth - blockerDepth) / blockerDepth * lightRadius
    float penumbra = (receiverDepth - avgBlocker) / max(avgBlocker, 1e-6) * lightRadius;

    // Map penumbra to a sensible UV radius range and clamp
    float radiusUV = clamp(penumbra, PCSS_MIN_RADIUS_UV, PCSS_MAX_RADIUS_UV);

    // 3) PCF using adaptive radius
    float shadow = pcfFilter(sm, uv, receiverDepth, layer, radiusUV);
    return shadow;
}
// --------------------- end PCSS helpers ---------------------

void main() {
vec2 uv = gl_FragCoord.xy / iResolution;

vec3 pos = texture2D(position,uv).xyz;
vec3 normal = texture2D(normalMapSampler,uv).xyz;

vec4 metalnessRoughness = texture2D(metallicRoughnessSampler,uv);
float metal = metalnessRoughness.x;
float roughness = metalnessRoughness.y;

// 从 G-Buffer 读取反照率（漫反射颜色）
vec3 albedo = texture2D(diffuseColorSampler, uv).xyz;

// 环境光分量（全局光照，不受阴影影响）
// 环境光只在循环外计算一次，避免重复累加
vec3 ambient = 0.1 * albedo;

// 初始化最终颜色为环境光
Color = vec4(ambient, 1.0);

for(int i = 0; i < light_count; i ++) {

// 计算光照向量
vec3 lightDir = normalize(lights[i].position - pos);
vec3 viewDir = normalize(camPos - pos);
vec3 halfDir = normalize(lightDir + viewDir);

// Blinn-Phong 光照计算
// 1. 漫反射分量：Lambert 余弦定律
float diff = max(dot(normal, lightDir), 0.0);
vec3 diffuse = diff * lights[i].color * albedo;

// 2. 高光分量：Blinn-Phong 模型
// 使用粗糙度计算高光指数（粗糙度越低，高光越集中）
float shininess = (1.0 - roughness) * 128.0;
float spec = pow(max(dot(normal, halfDir), 0.0), shininess);
vec3 specular = spec * lights[i].color;

// 3. 组合直接光照（漫反射 + 高光）
vec3 directLight = diffuse + specular;

// 4. 应用阴影（硬阴影实现）
// 将当前像素的世界位置转换到光源空间
vec4 lightSpacePos = lights[i].light_projection * lights[i].light_view * vec4(pos, 1.0);

// 执行透视除法，得到 NDC 坐标
vec3 lightSpaceNDC = lightSpacePos.xyz / lightSpacePos.w;

// 将 NDC 坐标从 [-1, 1] 映射到 [0, 1]（纹理坐标范围）
vec2 shadowUV = lightSpaceNDC.xy * 0.5 + 0.5;

// 将当前像素的 NDC 深度也映射到 [0, 1] 范围，与阴影深度图保持一致
float currentDepth = lightSpaceNDC.z * 0.5 + 0.5;

// 初始化阴影因子为 1.0（不在阴影中）
float shadowFactor = 1.0;

// 检查是否在阴影贴图的有效范围内
if (shadowUV.x >= 0.0 && shadowUV.x <= 1.0 && 
    shadowUV.y >= 0.0 && shadowUV.y <= 1.0 &&
    lightSpaceNDC.z >= -1.0 && lightSpaceNDC.z <= 1.0) {

    // 使用 PCSS 实现软阴影
    shadowFactor = computePCSS(shadow_maps, shadowUV, currentDepth, lights[i].shadow_map_id, lights[i].radius);
}

// 5. 应用阴影因子（只对直接光照应用阴影）
directLight *= shadowFactor;

// 6. 累加到最终颜色（多个光源）
Color += vec4(directLight, 1.0);
}

}