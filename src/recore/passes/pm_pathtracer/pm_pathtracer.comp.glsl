#version 460

#define ENABLE_RAY_TRACING

#include <recore/scene/scene.glsl>
#include <recore/scene/lights.glsl>
#include <recore/scene/shading.glsl>
#include <recore/scene/ray_tracing.glsl>

#include <recore/shaders/random.glsl>
#include <recore/shaders/hashgrid.glsl>
#include <recore/shaders/photon_mapping.glsl>


#include <recore/shaders/bsdf/frostbite.glsl>
#include <recore/shaders/bsdf/dielectric.glsl>

#include "pm_pathtracer.glslh"

layout(local_size_x = 32, local_size_y = 32, local_size_z = 1) in;

layout(set = 1, binding = 0, rgba32f) uniform image2D gOutputImage;

// GBuffer
layout(set = 1, binding = 1) uniform sampler2D gGPosition;
layout(set = 1, binding = 2) uniform sampler2D gGNormal;
layout(set = 1, binding = 3) uniform sampler2D gGAlbedo;
layout(set = 1, binding = 4) uniform sampler2D gGEmission;
layout(set = 1, binding = 5) uniform sampler2D gGMaterial;


PUSH_CONSTANT(PhotonMappingPathTracerPush);

struct BRDFSample {
  vec3 wo; // sampled outgoing direction
  float pdf; // pdf in solid angle
  vec3 weight; // brdf * cos_theta / pdf
};

vec3 evalBRDF(ShadingData sd, vec3 wi, vec3 wo) {
  if (sd.roughness == 0.f) {
    return vec3(0.f);
  }
  return Lambert_eval(sd, wo);
  // return Frostbite_eval(sd, wo, wi) * max(dot(sd.N, wo), 0);
}


BRDFSample sampleBRDF(inout RandomSampler rng, ShadingData sd, vec3 wi) {
  BRDFSample brdfSample;

  mat3 tangentToWorld = tangentFrame(normalize(sd.N));
  mat3 worldToTangent = inverse(tangentToWorld);

  if (sd.roughness < 0.05f) {

    // Handle dielectric material

    brdfSample.wo = normalize(tangentToWorld * Dielectric_sample(rng, normalize(worldToTangent * wi), sd.ior));
    brdfSample.pdf = 1.f;
    brdfSample.weight = vec3(1.f);

  } else {

    // Handle microfacet material

    DirectionalSample ds;
    sampleCosineHemisphere(rng, ds);
    // ds = Frostbite_sample(rng, sd, worldToTangent * wi);

    brdfSample.wo = normalize(tangentToWorld * normalize(ds.wo));
    brdfSample.pdf = ds.pdf;
    brdfSample.weight = evalBRDF(sd, wi, brdfSample.wo) / brdfSample.pdf;


  }




  return brdfSample;
}

float pdfBRDF(ShadingData sd, vec3 wi, vec3 wo) {
  if (dot(sd.N, wo) <= 0) {
    return 0.0;
  }

  return pdfSampleCosineHemisphere(max(0, dot(normalize(sd.N), normalize(wo))));
}


vec3 evalNEE(inout RandomSampler rng, ShadingData sd, vec3 wi) {
#if 0
  Light light = deref(p.scene.lights, 0);
  const DirectionalLight sunLight = DirectionalLight(light.direction, light.color, light.intensity);

  vec3 wo = -sunLight.direction;
  vec3 lambert = evalBRDF(sd, normalize(wo), normalize(wi));
  vec3 lightWeight = DirectionalLight_eval(sunLight);
  float shadow = float(!traceShadowRay(Ray(sd.P, 1000 * wo)));

  return 10.f* lambert * lightWeight * shadow;
#else
  const vec3 SPHERE_LIGHT = vec3(4.f, 3.0f, -1.f);
  const float SPHERE_LIGHT_RADIUS = 0.3f;

  vec3 P = SPHERE_LIGHT + SPHERE_LIGHT_RADIUS * sampleSphere(rng);
  vec3 wo = P - sd.P;
  float shadow2 = float(!traceShadowRay(Ray(sd.P, wo)));

  float falloff = 1.f / dot(wo, wo);
  return 5.f* evalBRDF(sd, normalize(wi), normalize(wo)) * shadow2 * falloff / pdfSampleSphere();
#endif
}

vec3 evalEmissive(ShadingData sd) {
  return sd.emission;
}


#define PATH_TRACER_MAX_BOUNCES 5

vec3 tracePath(inout RandomSampler rng, ivec2 pixel, ShadingData sd, vec3 wi) {
  vec3 contribution = vec3(0.0);
  vec3 throughput = vec3(1.0);

  for (uint i = 0; i < PATH_TRACER_MAX_BOUNCES; i++) {
    // NEE - Direct Illumination (analytic lights only)
    vec3 nee = evalNEE(rng, sd, wi);
    contribution += throughput * nee;

    // Emissive surface - randomly hit a light source
    vec3 emission = evalEmissive(sd);
    contribution += throughput * emission;

    // BSDF - Sample outgoing direction
    BRDFSample brdfSample = sampleBRDF(rng, sd, wi);
    throughput *= brdfSample.weight;

    // Trace path
    Ray ray = Ray(sd.P + sign(dot(brdfSample.wo, sd.N)) * M_EPSILON * sd.N, brdfSample.wo);
    Hit hit;
    if(!traceRay(ray, hit)) {
      break;
    }

    sd = queryShadingData(p.scene, hit.instanceID, hit.primitiveID, hit.barycentrics);
    wi = -brdfSample.wo;
  }
  return contribution;
}

vec3 jitter(inout RandomSampler rng, vec3 T, vec3 B) {
  float r = RandomSampler_nextFloat(rng);
  float phi = 2.f * M_PI * RandomSampler_nextFloat(rng);
  vec2 offset = r * vec2(cos(phi), sin(phi));
  return T * offset.x + B * offset.y;
}

void main() {
  ivec2 pixel = ivec2(gl_GlobalInvocationID.xy);
  ivec2 resolution = imageSize(gOutputImage);

  if (any(greaterThanEqual(pixel, resolution))) {
    return;
  }

  RandomSampler rng = RandomSampler_init(pixel, resolution, p.rngSeed, p.scene.frameCount);

  // Load GBuffer for primary shading data

  ShadingData sd;
  sd.P = texelFetch(gGPosition, pixel, 0).xyz;
  sd.N = normalize(texelFetch(gGNormal, pixel, 0).xyz);
  sd.albedo = texelFetch(gGAlbedo, pixel, 0).xyz;
  sd.emission = texelFetch(gGEmission, pixel, 0).xyz;
  vec4 material = texelFetch(gGMaterial, pixel, 0);
  sd.metallic = material.z;
  sd.roughness = material.y;
  sd.ior = material.a;

  // #define DEBUG_OUTPUT_ALBEDO
  #ifdef DEBUG_OUTPUT_ALBEDO
  imageStore(gOutputImage, pixel, vec4(sd.albedo, 1.0));
  return;
  #endif

  vec3 color = vec3(0.0);
  vec3 wi = normalize(p.scene.camera.position - sd.P);
  color = tracePath(rng, pixel, sd, wi);

  mat3 TBN = tangentFrame(sd.N);
  vec3 jitterPosition = sd.P + 2.f * p.hashGrid.scale * jitter(rng, TBN[0], TBN[1]);
  uint key = HashGrid_find(p.hashGrid, HashGrid_hash(p.hashGrid, jitterPosition, sd.N));
  if (key != -1) {
    PhotonCell cell = deref(p.photons, key);
    color += evalBRDF(sd, wi, normalize(cell.direction)) * cell.flux / (float(deref(p.photons, 0).counter) * p.hashGrid.scale * p.hashGrid.scale);
  }

  imageStore(gOutputImage, pixel, vec4(color, 1.0));
}
