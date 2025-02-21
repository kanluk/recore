#version 460

#define ENABLE_RAY_TRACING

#include <recore/scene/scene.glsl>
#include <recore/scene/lights.glsl>
#include <recore/scene/shading.glsl>
#include <recore/scene/ray_tracing.glsl>
#include <recore/shaders/random.glsl>

#include "diffuse_pathtracer.glslh"

layout(local_size_x = 32, local_size_y = 32, local_size_z = 1) in;

layout(set = 1, binding = 0, rgba32f) uniform image2D gOutputImage;

// GBuffer
layout(set = 1, binding = 1) uniform sampler2D gGPosition;
layout(set = 1, binding = 2) uniform sampler2D gGNormal;
layout(set = 1, binding = 3) uniform sampler2D gGAlbedo;
layout(set = 1, binding = 4) uniform sampler2D gGEmission;


PUSH_CONSTANT(DiffusePathTracerPush);


vec3 evalNEE(ShadingData sd, vec3 wi) {
  Light light = deref(p.scene.lights, 0);
  const DirectionalLight sunLight = DirectionalLight(light.direction, light.color, light.intensity);

  vec3 wo = -sunLight.direction;
  vec3 lambert = Lambert_eval(sd, normalize(wo));
  vec3 lightWeight = DirectionalLight_eval(sunLight);
  float shadow = float(!traceShadowRay(Ray(sd.P, 1000 * wo)));

  return lambert * lightWeight * shadow;
}

vec3 evalEmissive(ShadingData sd) {
  return sd.emission;
}

struct BRDFSample {
  vec3 wo; // sampled outgoing direction
  float pdf; // pdf in solid angle
  vec3 weight; // brdf * cos_theta / pdf
};

BRDFSample sampleBRDF(inout RandomSampler rng, ShadingData sd, vec3 wi) {
  BRDFSample brdfSample;

  DirectionalSample ds;
  sampleCosineHemisphere(rng, ds);

  // Transform wo to world space
  mat3 tangentToWorld = tangentFrame(normalize(sd.N));

  brdfSample.wo = normalize(tangentToWorld * normalize(ds.wo));
  brdfSample.pdf = ds.pdf;
  brdfSample.weight = Lambert_eval(sd, brdfSample.wo) / brdfSample.pdf;

  return brdfSample;
}

float pdfBRDF(ShadingData sd, vec3 wi, vec3 wo) {
  if (dot(sd.N, wo) <= 0) {
    return 0.0;
  }

  return pdfSampleCosineHemisphere(max(0, dot(normalize(sd.N), normalize(wo))));
}

#define PATH_TRACER_MAX_BOUNCES 5


vec3 tracePath(inout RandomSampler rng, ivec2 pixel, ShadingData sd, vec3 wi) {
  vec3 contribution = vec3(0.0);
  vec3 throughput = vec3(1.0);

  for (uint i = 0; i < PATH_TRACER_MAX_BOUNCES; i++) {
    // NEE - Direct Illumination (analytic lights only)
    vec3 nee = evalNEE(sd, wi);
    contribution += throughput * nee;

    // Emissive surface - randomly hit a light source
    vec3 emission = evalEmissive(sd);
    contribution += throughput * emission;

    // BSDF - Sample outgoing direction
    BRDFSample brdfSample = sampleBRDF(rng, sd, wi);
    throughput *= brdfSample.weight;

    // Trace path
    Ray ray = Ray(sd.P + M_EPSILON * sd.N, brdfSample.wo);
    Hit hit;
    if(!traceRay(ray, hit)) {
      break;
    }

    sd = queryShadingData(p.scene, hit.instanceID, hit.primitiveID, hit.barycentrics);
    wi = -brdfSample.wo;
  }
  return contribution;
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

  // #define DEBUG_OUTPUT_ALBEDO
  #ifdef DEBUG_OUTPUT_ALBEDO
  imageStore(gOutputImage, pixel, vec4(sd.albedo, 1.0));
  return;
  #endif

  vec3 color = vec3(0.0);
  vec3 wi = normalize(p.scene.camera.position - sd.P);
  color = tracePath(rng, pixel, sd, wi);
  imageStore(gOutputImage, pixel, vec4(color, 1.0));
}
