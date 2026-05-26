#version 430 core

uniform mat4 light_view;
uniform mat4 light_projection;
in vec3 vertexPosition;
layout(location = 0) out float shadow_map0;

void main() {
    vec4 clipPos = light_projection * light_view * (vec4(vertexPosition, 1.0));
    
    // 执行透视除法，得到 NDC 深度值 [-1, 1]
    float ndcDepth = clipPos.z / clipPos.w;
    
    // 将 NDC 深度映射到 [0, 1] 范围，以匹配纹理存储格式
    shadow_map0 = ndcDepth * 0.5 + 0.5;
} 