#version 450

layout (location = 0) out vec4 outColor;

layout (location = 0) in vec2 inTexCoord;

layout (set = 0, binding = 0) uniform sampler2D samplerTexture;

void main() {
  outColor = vec4(1.0, 0.753, 0.796, 1.0);// Pink for now

  outColor = vec4(texture(samplerTexture, inTexCoord).xyz, 1.0);
}
