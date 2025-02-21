#version 450

#include "rsm.glslh"

#include <recore/scene/scene.glsl>

layout(location = 0) in vec3 inPosition;
layout(location = 1) in vec3 inNormal;
layout(location = 2) in vec2 inTexCoord;

layout(location = 0) out vec3 outPosition;
layout(location = 1) out vec3 outNormal;
layout(location = 2) out vec2 outTexCoord;

PUSH_CONSTANT(RSMPush);


void main() {
  GeometryInstance geometryInstance = deref(p.scene.geometryInstances, p.geometryInstanceID);
  mat4 model = deref(p.scene.modelMatrices, geometryInstance.modelMatrixID);
  outPosition = vec3(model * vec4(inPosition, 1.0));

  Light light = deref(p.scene.lights, 0);
  gl_Position = light.shadowMatrix * vec4(outPosition, 1.0);

  mat3 normalMatrix = inverse(transpose(mat3(model)));
  outNormal = normalMatrix * inNormal;

  outTexCoord = inTexCoord;
}
