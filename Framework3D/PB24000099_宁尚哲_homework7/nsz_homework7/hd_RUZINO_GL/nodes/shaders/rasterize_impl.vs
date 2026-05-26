#version 430 core
layout(location = 0) in vec3 aPos;
layout(location = 1) in vec3 aNormal;
layout(std430, binding = 0) buffer buffer0 {
	vec2 data[];
} aTexcoord;

out vec3 vertexPosition;
out vec3 vertexNormal;
out vec2 vTexcoord;

uniform mat4 model;
uniform mat4 view;
uniform mat4 projection;

// Displacement map uniforms
uniform sampler2D displacementSampler;
uniform float displacementScale;
uniform float displacementBias;

void main() {
	// fetch texcoord early (SSBO based)
	vec2 uv = aTexcoord.data[gl_VertexID];
	uv.y = 1.0 - uv.y;

	// sample height and displace along the model-space normal
	float height = texture(displacementSampler, uv).r;
	vec3 normalModel = normalize(aNormal);
	vec3 displacedModelPos = aPos + normalModel * (height * displacementScale + displacementBias);

	// transform to world/clip space
	vec4 worldPos = model * vec4(displacedModelPos, 1.0);
	gl_Position = projection * view * worldPos;
	vertexPosition = worldPos.xyz;
	vertexNormal = normalize(inverse(transpose(mat3(model))) * aNormal);
	vTexcoord = uv;
}