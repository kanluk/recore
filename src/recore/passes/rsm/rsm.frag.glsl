#version 450

#include "rsm.glslh"

#include <recore/scene/scene.glsl>


layout(location = 0) in vec3 inPosition;
layout(location = 1) in vec3 inNormal;
layout(location = 2) in vec2 inTexCoord;

layout(location = 0) out vec4 outRSMPosition;
layout(location = 1) out vec4 outRSMNormal;
layout(location = 2) out vec4 outRSMFlux;

PUSH_CONSTANT(RSMPush);

void main() {
  GeometryInstance geometryInstance = deref(p.scene.geometryInstances, p.geometryInstanceID);
  Material material = deref(p.scene.materials, geometryInstance.materialID);

  vec4 baseColor = material.baseColorFactor;
  if (material.baseColorID != -1) {
    baseColor *= texture(gTextures[material.baseColorID], inTexCoord);
  }

  vec3 N = normalize(inNormal);

  Light light = deref(p.scene.lights, 0);
  vec3 flux = light.intensity * max(0, dot(normalize(inNormal), -normalize(light.direction))) * baseColor.rgb;
  float distance = distance(light.position, inPosition);

  outRSMPosition = vec4(inPosition, distance);
  outRSMNormal = vec4(N, 1.0);
  outRSMFlux = vec4(flux, 1.0);
}
