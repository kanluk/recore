#ifndef SHADING_GLSL
#define SHADING_GLSL

#include <recore/shaders/constants.glsl>
#include <recore/shaders/math.glsl>

#include <recore/scene/scene.glsl>


struct ShadingData {
  vec3 P;
  vec3 N;
  vec3 albedo;
  vec3 emission;
  float metallic;
  float roughness;
  float ior;
};

ShadingData queryShadingData(SceneData scene, int instanceID, int primitiveID, vec2 barycentrics) {
  GeometryInstance instance = deref(scene.geometryInstances, instanceID);
  Mesh mesh = deref(scene.meshes, instance.meshID);
  Material material = deref(scene.materials, instance.materialID);

  // Query vertices
  Vertex v0 = deref(scene.vertices, deref(scene.indices, 3 * primitiveID + mesh.firstIndex + 0));
  Vertex v1 = deref(scene.vertices, deref(scene.indices, 3 * primitiveID + mesh.firstIndex + 1));
  Vertex v2 = deref(scene.vertices, deref(scene.indices, 3 * primitiveID + mesh.firstIndex + 2));

  // Interpolate attributes
  vec3 b = vec3(1 - barycentrics.x - barycentrics.y, barycentrics.x, barycentrics.y);
  vec3 position = v0.position * b.x + v1.position * b.y + v2.position * b.z;
  vec3 normal = normalize(v0.normal * b.x + v1.normal * b.y + v2.normal * b.z);
  vec2 texCoord = v0.texCoord * b.x + v1.texCoord * b.y + v2.texCoord * b.z;

  // TODO: maybe try VK_KHR_ray_tracing_position_fetch in the future
  // Transform to world space
  mat4 model = deref(scene.modelMatrices, instance.modelMatrixID);
  mat3 modelNormal = inverse(transpose(mat3(model)));

  position = (model * vec4(position, 1.0)).xyz;
  normal = normalize(modelNormal * normal);

  vec4 albedo = material.baseColorFactor;
  if (material.baseColorID != -1) {
    albedo *= texture(gTextures[material.baseColorID], texCoord);
  }

  float metallic = material.metallicFactor;
  float roughness = material.roughnessFactor;
  if (material.metallicRoughnessID != -1) {
    vec4 metallicRoughness = texture(gTextures[material.metallicRoughnessID], texCoord);
    metallic *= metallicRoughness.b;
    roughness *= metallicRoughness.g;
  }

  ShadingData sd;
  sd.P = position;
  sd.N = normal;
  sd.albedo = albedo.xyz;
  sd.emission = material.emissiveFactor.xyz;
  sd.metallic = metallic;
  sd.roughness = roughness;
  sd.ior = material.ior;

  return sd;
}


vec3 Lambert_eval(ShadingData sd, vec3 wo) {
  return sd.albedo * M_1_PI * max(0.0, dot(sd.N, wo));
}

#endif // SHADING_GLSL
