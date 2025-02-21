#version 450

#include "tone_mapping.glslh"

layout (location = 0) in vec2 inTexCoord;

layout (location = 0) out vec4 outColor;

layout (set = 0, binding = 0) uniform sampler2D gInputSampler;

PUSH_CONSTANT(ToneMappingPush);

void main() {
  vec3 color = texture(gInputSampler, inTexCoord).xyz;

  color *= p.exposure;
  color = pow(color, vec3(1.f / p.gamma));

  outColor = vec4(color, 1.f);
}
