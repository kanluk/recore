#version 460

layout(location = 0) in vec3 inPosition; // In world space already

layout(location = 0) out vec3 outPosition;

layout(push_constant) uniform Push {
  mat4 viewProjection;
};

void main() {
  outPosition = inPosition;
  gl_Position = viewProjection * vec4(outPosition, 1.0);
}
