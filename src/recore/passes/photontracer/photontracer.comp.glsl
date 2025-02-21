#version 460

#extension GL_EXT_shader_atomic_float : require

#define ENABLE_RAY_TRACING

#include <recore/scene/scene.glsl>
#include <recore/scene/lights.glsl>
#include <recore/scene/shading.glsl>
#include <recore/scene/ray_tracing.glsl>
#include <recore/shaders/random.glsl>

#include <recore/shaders/atomic.glsl>


#include <recore/shaders/bsdf/dielectric.glsl>

#include <recore/shaders/photon_mapping.glsl>
#include <recore/shaders/hashgrid.glsl>


#include "photontracer.glslh"


layout(local_size_x = 32, local_size_y = 1, local_size_z = 1) in;

PUSH_CONSTANT(PhotonTracerPush);

const vec3 SPHERE_LIGHT = vec3(4.f, 3.0f, -1.f);
const float SPHERE_LIGHT_RADIUS = 0.3f;


vec3 samplePhotonOriginSphere(inout RandomSampler rng, out ShadingData sd, out vec3 wo) {
  sd.P = SPHERE_LIGHT + SPHERE_LIGHT_RADIUS * sampleSphere(rng);
  wo = normalize(SPHERE_LIGHT - sd.P);
  sd.N = wo;

  vec3 flux = vec3(1.f);
  flux *= (M_PI * 4.f * (SPHERE_LIGHT_RADIUS * SPHERE_LIGHT_RADIUS)) / pdfSampleSphere();
  flux *= abs(dot(normalize(sd.P - SPHERE_LIGHT), wo));
  return 5.f * flux;
}

vec3 samplePhotonOrigin(inout RandomSampler rng, out ShadingData sd, out vec3 wo) {
  // TODO: simply use light 0 for now
  Light light = deref(p.scene.lights, 0);

  vec2 rnd = 2.f * RandomSampler_nextVec2(rng) - 1.f;

  mat3 TBN = tangentFrame(light.direction);
  const float radius = 10.f;

  sd.P = light.position + radius * (rnd.x * TBN[0] + rnd.y * TBN[1]);
  sd.N = normalize(light.direction);
  wo = normalize(light.direction);

  return (light.color * light.intensity) * (4.f * radius * radius);
}

void main() {
  uint globalIdx = gl_GlobalInvocationID.x;
  RandomSampler rng = RandomSampler_init(globalIdx, 4096 * p.scene.frameCount);

  ShadingData sd;
  vec3 wo;

  // vec3 flux = samplePhotonOrigin(rng, sd, wo);
  vec3 flux = samplePhotonOriginSphere(rng, sd, wo);

  // 3 is enough for glass sphere: light -> enter sphere -> leave sphere -> diffuse hit
  for (uint i =  0; i < 3; i++) {
    if (i > 0 && sd.roughness > 0.05f) {
      break;
    }

    Ray ray = Ray(sd.P + sign(dot(wo, sd.N)) * M_EPSILON * sd.N, wo);
    Hit hit;
    if (!traceRay(ray, hit)) {
      break;
    }

    sd = queryShadingData(p.scene, hit.instanceID, hit.primitiveID, hit.barycentrics);

    // If surface is diffuse: place photon
    if (i > 0 && sd.roughness > 0.05f) {
      uint key = HashGrid_findOrInsert(p.hashGrid, HashGrid_hash(p.hashGrid, sd.P, sd.N));

      atomicAdd(deref(p.photons, 0).counter, 1);

      #define cell deref(p.photons, key)
      atomicAdd(cell.counter, 1);
      atomicAddVec3(cell.flux, flux);
      atomicAddVec3(cell.direction, -wo);

      #undef cell
      return;
    }

    mat3 tangentToWorld = tangentFrame(normalize(sd.N));
    mat3 worldToTangent = inverse(tangentToWorld);

    vec3 wi = -wo;

    wo = tangentToWorld * Dielectric_sample(rng, worldToTangent * wi, sd.ior);
  }
}
