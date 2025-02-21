#version 460

#include <recore/scene/scene.glsl>

layout(location = 0) in vec3 inPosition;
layout(location = 1) in vec3 inNormal;
layout(location = 2) in vec2 inTexCoord;
layout(location = 3) in vec4 inPositionClip;
layout(location = 4) in vec4 inPrevPositionClip;

layout(location = 0) out vec4 outGPosition;
layout(location = 1) out vec4 outGNormal;
layout(location = 2) out vec4 outGAlbedo;
layout(location = 3) out vec4 outGEmissive;
layout(location = 4) out vec2 outGMotion;
layout(location = 5) out vec4 outGMaterial;


layout(push_constant) uniform Push {
  SceneData scene;
  uint geometryInstanceID;
};

// #define USE_FACE_NORMAL

vec3 getFaceNormal(uint instanceID, uint primitiveID, uint meshID, uint modelMatrixID) {
  Mesh mesh = deref(scene.meshes, meshID);
  mat4 model = deref(scene.modelMatrices, modelMatrixID);
  mat3 normalMatrix = inverse(transpose(mat3(model)));

  Vertex v0 = deref(scene.vertices, deref(scene.indices, 3 * primitiveID + mesh.firstIndex + 0));
  Vertex v1 = deref(scene.vertices, deref(scene.indices, 3 * primitiveID + mesh.firstIndex + 1));
  Vertex v2 = deref(scene.vertices, deref(scene.indices, 3 * primitiveID + mesh.firstIndex + 2));

  vec3 N = cross(v1.position - v0.position, v2.position - v0.position);
  return normalize(normalMatrix * N);
}


// #define ENABLE_NORMAL_MAPPING

// http://www.thetenthplanet.de/archives/1180
vec3 normalMapping(vec3 N, vec3 texN) {
  texN = texN * 2 - 1;

  // get edge vectors of the pixel triangle
  vec3 dp1 = dFdx(inPosition);
  vec3 dp2 = dFdy(inPosition);
  vec2 duv1 = dFdx(inTexCoord);
  vec2 duv2 = dFdy(inTexCoord);

  // solve the linear system
  vec3 dp2perp = cross(dp2, N);
  vec3 dp1perp = cross(N, dp1);
  vec3 T = dp2perp * duv1.x + dp1perp * duv2.x;
  vec3 B = dp2perp * duv1.y + dp1perp * duv2.y;

  // construct a scale-invariant frame
  float invmax = inversesqrt(max(dot(T, T), dot(B, B)));
  mat3 TBN = mat3(-T * invmax, B * invmax, N);
  return TBN * texN;
}


void main() {
  GeometryInstance geometryInstance = deref(scene.geometryInstances, geometryInstanceID);
  Material material = deref(scene.materials, geometryInstance.materialID);

  vec3 N = normalize(inNormal);

#ifdef ENABLE_NORMAL_MAPPING
  if (material.normalMapID != -1) {
    vec3 texN = texture(gTextures[material.normalMapID], inTexCoord).rgb;
    N = normalMapping(N, texN);
  }
#endif

#ifdef USE_FACE_NORMAL
  N = getFaceNormal(geometryInstanceID, gl_PrimitiveID, geometryInstance.meshID, geometryInstance.modelMatrixID);
#endif

  vec4 baseColor = material.baseColorFactor;
  if (material.baseColorID != -1) {
    baseColor *= texture(gTextures[material.baseColorID], inTexCoord);
  }

  float metallic = material.metallicFactor;
  float roughness = material.roughnessFactor;
  if (material.metallicRoughnessID != -1) {
    vec4 metallicRoughness = texture(gTextures[material.metallicRoughnessID], inTexCoord);
    metallic *= metallicRoughness.b;
    roughness *= metallicRoughness.g;
  }

  outGPosition = vec4(inPosition, 1.0);
  outGNormal = vec4(N, 1.0);
  outGAlbedo = baseColor;
  outGEmissive = material.emissiveFactor;
  outGMaterial = vec4(0.0, roughness, metallic, material.ior);

  vec2 currentPosition = inPositionClip.xy / inPositionClip.w;
  vec2 prevPosition = inPrevPositionClip.xy / inPrevPositionClip.w;
  outGMotion = (prevPosition - currentPosition) * 0.5f;
}
