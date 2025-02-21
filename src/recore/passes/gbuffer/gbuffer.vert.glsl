#version 450

#include <recore/scene/scene.glsl>

layout(location = 0) in vec3 inPosition;
layout(location = 1) in vec3 inNormal;
layout(location = 2) in vec2 inTexCoord;

layout(location = 0) out vec3 outPosition;
layout(location = 1) out vec3 outNormal;
layout(location = 2) out vec2 outTexCoord;

layout(location = 3) out vec4 outPositionClip;
layout(location = 4) out vec4 outPrevPositionClip;

layout(push_constant) uniform Push {
  SceneData scene;
  uint geometryInstanceID;
};

void main() {
  GeometryInstance geometryInstance = deref(scene.geometryInstances, geometryInstanceID);
  mat4 model = deref(scene.modelMatrices, geometryInstance.modelMatrixID);
  outPosition = vec3(model * vec4(inPosition, 1.0));

  mat3 normalMatrix = inverse(transpose(mat3(model)));
  outNormal = normalMatrix * inNormal;

  outTexCoord = inTexCoord;

  outPositionClip = scene.camera.viewProjection * vec4(outPosition, 1.0);
  outPrevPositionClip = scene.camera.prevViewProjection * vec4(outPosition, 1.0);

  gl_Position = outPositionClip;
}
