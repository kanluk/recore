#version 460

#define ENABLE_RAY_TRACING

#extension GL_EXT_debug_printf : require
#extension GL_ARB_gpu_shader_int64 : require

#extension GL_EXT_shader_atomic_float : require


#include <recore/scene/scene.glsl>
#include <recore/scene/lights.glsl>
#include <recore/scene/shading.glsl>
#include <recore/scene/ray_tracing.glsl>

#include <recore/shaders/random.glsl>
#include <recore/shaders/hashgrid.glsl>
#include <recore/shaders/visual_debug.glsl>
#include <recore/shaders/vmm.glsl>

#include <recore/shaders/math.glsl>


#include "guided_pathtracer.glslh"



layout(local_size_x = 32, local_size_y = 32, local_size_z = 1) in;

layout(set = 1, binding = 0, rgba32f) uniform image2D gOutputImage;

layout(set = 1, binding = 1) uniform sampler2D gGPosition;
layout(set = 1, binding = 2) uniform sampler2D gGNormal;
layout(set = 1, binding = 3) uniform sampler2D gGAlbedo;
layout(set = 1, binding = 4) uniform sampler2D gGEmission;


PUSH_CONSTANT(GuidedPathTracerPush);

// const PointLight light = PointLight(vec3(0.0, 2.0, 0.0), vec3(1.0, 1.0, 1.0), 10000.0);
// const PointLight light = PointLight(vec3(0.0, 1.9, 0.0), vec3(1.0, 1.0, 1.0), 3.0);
// const DirectionalLight sunLight = DirectionalLight(vec3(0.0, -2.0, -1.0), vec3(1.0, 1.0, 1.0), 50.f);

vec3 evalNEE(ShadingData sd, vec3 wi) {
  #if 0

  vec3 wo = light.position - sd.P;
  vec3 lambert = Lambert_eval(sd, normalize(wo));
  vec3 lightWeight = PointLight_eval(light, sd.P);
  float shadow = float(!traceShadowRay(Ray(sd.P, wo)));

  #else

  Light light = deref(p.scene.lights, 0);
  const DirectionalLight sunLight = DirectionalLight(light.direction, light.color, light.intensity);

  vec3 wo = -sunLight.direction;
  vec3 lambert = Lambert_eval(sd, normalize(wo));
  vec3 lightWeight = DirectionalLight_eval(sunLight);
  float shadow = float(!traceShadowRay(Ray(sd.P, 1000 * wo)));

  #endif

  return lambert * lightWeight * shadow;

  // return vec3(0.0);
}

vec3 evalEmissive(ShadingData sd) {
  return sd.emission;
  // return vec3(0.0);
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





struct GuidingTrainPathVertex {
  vec3 position;
  vec3 normal;
  vec3 direction;
  vec3 neeWeight;
  vec3 brdfWeight;
  uint key;
};

// Balance Heuristic
float evalMIS(float pdf0, float pdf1) {
  return pdf0 / (pdf0 + pdf1);
}

float evalMISOneSample(float pdf0, float pdf1, float pdfSelection) {
  if (pdfSelection == 1.f) {
    return 1.f;
  }
  return evalMIS(pdf0 * pdfSelection, pdf1 * (1.f - pdfSelection)) / pdfSelection;
}



vec3 tracePathTrainGuide(inout RandomSampler rng, ivec2 pixel, ShadingData sd, vec3 wi) {
  vec3 contribution = vec3(0.0);
  vec3 throughput = vec3(1.0);

  GuidingTrainPathVertex guideTrainSample[PATH_TRACER_MAX_BOUNCES];

  // Generate path from camera
  int i = 0;
  for (; i < PATH_TRACER_MAX_BOUNCES; i++) {
    // NEE - Direct Illumination
    vec3 nee = evalNEE(sd, wi);
    contribution += throughput * nee;

    // Emissive surfaces
    vec3 emission = evalEmissive(sd);
    contribution += throughput * emission;

    // BSDF - Sample outgoing direction
    BRDFSample brdfSample = sampleBRDF(rng, sd, wi);

    // Acquire hash grid index and look at guiding mixture model
    uint key = HashGrid_findOrInsert(p.hashGrid, HashGrid_hash(p.hashGrid, sd.P, sd.N));
    VMM vmm = deref(p.vmms, key);

    // Do some guiding ...
    if (p.guide && !VMM_isEmpty(vmm)) {
      // ... based on the VMM

      // MIS between BRDF and VMM
      if (RandomSampler_nextFloat(rng) < p.selectGuidingProbability) {
        DirectionalSample ds;
        VMM_sample(vmm, rng, ds);
        brdfSample.wo = ds.wo;
        float brdfPdf = pdfBRDF(sd, wi, brdfSample.wo);
        brdfSample.pdf = ds.pdf;
        brdfSample.weight = Lambert_eval(sd, brdfSample.wo) / brdfSample.pdf;

        brdfSample.weight *= evalMISOneSample(brdfSample.pdf, brdfPdf, p.selectGuidingProbability);
      } else {
        brdfSample.weight *= evalMISOneSample(brdfSample.pdf, VMM_pdf(vmm, brdfSample.wo), 1.f - p.selectGuidingProbability);
      }
    }

    throughput *= brdfSample.weight;

    // Store path vertex data for backwards pass
    guideTrainSample[i].position = sd.P;
    guideTrainSample[i].normal = sd.N;
    guideTrainSample[i].direction = brdfSample.wo;
    guideTrainSample[i].neeWeight = nee + emission;
    guideTrainSample[i].brdfWeight = brdfSample.weight;
    guideTrainSample[i].key = key;

    // Trace next path segment
    Ray ray = Ray(sd.P + M_EPSILON * sd.N, brdfSample.wo);
    Hit hit;
    if(!traceRay(ray, hit)) {
      break;
    }

    // Query next surface data and update incoming direction
    sd = queryShadingData(p.scene, hit.instanceID, hit.primitiveID, hit.barycentrics);
    wi = -brdfSample.wo;
  }

  // Backwards pass to collect guiding samples
  ivec2 resolution = imageSize(gOutputImage);
  uint baseIndex = (pixel.y * resolution.x + pixel.x) * PATH_TRACER_MAX_BOUNCES;

  i--;
  vec3 invContribution = guideTrainSample[i+1].neeWeight;
  for (; i >= 0; i--) {
    invContribution = guideTrainSample[i].brdfWeight * invContribution;
    vec3 guidingDirection = guideTrainSample[i].direction;
    float guidingWeight = average(invContribution);
    invContribution += guideTrainSample[i].neeWeight;

    uint key = guideTrainSample[i].key;
    if (key != -1) {
      uint currentFrame = p.scene.frameCount;
      uint lastAccess = atomicExchange(deref(p.hashGrid.cells, key).lastInsert, currentFrame);

      // Initialize if its the first access
      if (lastAccess == 0) {
        VMM vmm = VMM_initRandom(rng);
        deref(p.vmms, key) = vmm;
      }

      if (p.train) {
        uint indexInCell = atomicAdd(deref(p.cellCounters, key), 1);
        GuidingSample gs = GuidingSample(guidingDirection, guidingWeight, key, indexInCell);
        uint index = baseIndex + i;
        deref(p.guidingSamples, index) = gs;
      }
    }
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

  vec4 position = texelFetch(gGPosition, pixel, 0);
  vec4 normal = texelFetch(gGNormal, pixel, 0);
  vec4 albedo = texelFetch(gGAlbedo, pixel, 0);
  vec4 emission = texelFetch(gGEmission, pixel, 0);

  #if 0
  imageStore(gOutputImage, pixel, vec4(albedo.xyz, 1.0));
  return;
  #endif

  if (position.w < 1.0) {
    imageStore(gOutputImage, pixel, vec4(0.0, 0.0, 0.0, 1.0));
    return;
  }


  ShadingData sd;
  sd.P = position.xyz;
  sd.N = normalize(normal.xyz);
  sd.albedo = albedo.xyz;
  sd.emission = emission.xyz;

  vec3 color = vec3(0.0);
  vec3 wi = normalize(p.scene.camera.position - sd.P);

  color += tracePathTrainGuide(rng, pixel, sd, wi);

  imageStore(gOutputImage, pixel, vec4(color, 1.0));
}
