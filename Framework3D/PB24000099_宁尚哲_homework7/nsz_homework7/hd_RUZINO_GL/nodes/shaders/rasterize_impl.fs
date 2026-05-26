#version 430

layout(location = 0) out vec3 position;
layout(location = 1) out float depth;
layout(location = 2) out vec2 texcoords;
layout(location = 3) out vec3 diffuseColor;
layout(location = 4) out vec2 metallicRoughness;
layout(location = 5) out vec3 normal;

in vec3 vertexPosition;
in vec3 vertexNormal;
in vec2 vTexcoord;
uniform mat4 projection;
uniform mat4 view;

uniform sampler2D diffuseColorSampler;
 
// This only works for current scenes provided by the TAs 
// because the scenes we provide is transformed from gltf
uniform sampler2D normalMapSampler;
uniform sampler2D metallicRoughnessSampler;

void main() {
    // 计算顶点位置和深度
    position = vertexPosition;
    vec4 clipPos = projection * view * (vec4(position, 1.0));
    depth = clipPos.z / clipPos.w;
    // 采样顶点纹理坐标
    texcoords = vTexcoord;

    diffuseColor = texture2D(diffuseColorSampler, vTexcoord).xyz;
    metallicRoughness = texture2D(metallicRoughnessSampler, vTexcoord).zy;

    // 采样法线贴图解码，将[0,1]范围映射到[-1,1]
    vec3 normalmap_value = texture2D(normalMapSampler, vTexcoord).xyz;
    // my work: 计算切线空间法线并转换到世界空间
    vec3 tangent_normal = normalize(normalmap_value * 2.0 - 1.0);
    
    // 获取顶点法线
    normal = normalize(vertexNormal);

    // 计算切线和副切线，使用屏幕空间导数计算切线
    vec3 edge1 = dFdx(vertexPosition);
    vec3 edge2 = dFdy(vertexPosition);
    vec2 deltaUV1 = dFdx(vTexcoord);
    vec2 deltaUV2 = dFdy(vTexcoord);

    vec3 tangent = edge1 * deltaUV2.y - edge2 * deltaUV1.y; 

    // 鲁棒的切线和副切线计算，避免法线方向错误
    if(length(tangent) < 1E-7) {
        vec3 bitangent = -edge1 * deltaUV2.x + edge2 * deltaUV1.x;
        tangent = normalize(cross(bitangent, normal));
    }
    tangent = normalize(tangent - dot(tangent, normal) * normal);
    vec3 bitangent = normalize(cross(tangent, normal));

    // 构建TBN矩阵，将切线空间法线转换到世界空间
    mat3 TBN = mat3(tangent, bitangent, normal);
    normal = normalize(TBN * tangent_normal);

}