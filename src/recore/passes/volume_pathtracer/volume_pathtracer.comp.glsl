#version 460

#define ENABLE_RAY_TRACING

#include <recore/scene/scene.glsl>
#include <recore/scene/lights.glsl>
#include <recore/scene/shading.glsl>
#include <recore/scene/ray_tracing.glsl>
#include <recore/shaders/random.glsl>

#include "volume_pathtracer.glslh"

layout(local_size_x = 32, local_size_y = 32, local_size_z = 1) in;

layout(set = 1, binding = 0, rgba32f) uniform image2D gOutputImage;

// GBuffer
layout(set = 1, binding = 1) uniform sampler2D gGPosition;
layout(set = 1, binding = 2) uniform sampler2D gGNormal;
layout(set = 1, binding = 3) uniform sampler2D gGAlbedo;
layout(set = 1, binding = 4) uniform sampler2D gGEmission;


PUSH_CONSTANT(VolumePathTracerPush);



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


struct MediumSample {
  float mu_a;
  float mu_s;
  vec3 emission;
};

float MediumSample_mu_t(MediumSample ms) {
  return ms.mu_a + ms.mu_s;
}

MediumSample sampleMedium(vec3 P) {
  MediumSample ms;
  ms.mu_a = 0.05f;
  ms.mu_s = 0.1f;
  ms.emission = vec3(0.f);
  return ms;
}

struct TransmittanceSample {
  float t; // distance
  float transmittance;
  float pdf;
};

float evalTransmittance(float t, float mu_t) {
  return exp(-mu_t * t);
}

TransmittanceSample sampleTransmittance(inout RandomSampler rng, float mu_t) {
  float rnd = RandomSampler_nextFloat(rng);

  TransmittanceSample ts;
  ts.t = -log(1.f - rnd) / mu_t;
  ts.transmittance = evalTransmittance(ts.t, mu_t);
  ts.pdf = mu_t * ts.transmittance;
  return ts;
}

// Henyey-Greenstein phase function
float HGPhase_eval(float g, float cos_theta) {
    float denom = 1 + g * g + 2 * g * cos_theta;
    return 1 / (4 * M_PI) * (1 - g * g) / (denom * sqrt(denom));
}

BRDFSample HGPhase_sample(inout RandomSampler rng, vec3 wo, float g) {
    float cos_theta;
    vec2 u = RandomSampler_nextVec2(rng);
    if (abs(g) < 1e-3)
        cos_theta = 1.0 - 2.0 * u[0];
    else {
        float sqr = (1.0 - g * g) / (1.0 + g - 2.0 * g * u[0]);
        cos_theta = -(1.0 + g * g - sqr * sqr) / (2.0 * g);
    }

    float sin_theta = sqrt(max(0.0, 1.0 - cos_theta * cos_theta));
    float phi = 2.0 * M_PI * u[1];

    mat3 TBN = tangentFrame(wo);

    vec3 tvec = TBN[0];
    vec3 bvec = TBN[1];

    float sin_phi = sin(phi);
    float cos_phi = cos(phi);

    vec3 inc_light = sin_theta * cos_phi * tvec +
                       sin_theta * sin_phi * bvec +
                       cos_theta * wo;

    float pdf = HGPhase_eval(g, cos_theta);

    BRDFSample brdfSample;
    brdfSample.wo = inc_light;
    brdfSample.pdf = pdf;
    brdfSample.weight = vec3(1.f);
    return brdfSample;
}

#define HG_FORWARDNESS 0.01f


vec3 evalNEE(ShadingData sd, vec3 wi) {
  Light light = deref(p.scene.lights, 0);
  const DirectionalLight sunLight = DirectionalLight(light.direction, light.color, light.intensity);

  vec3 wo = -sunLight.direction;
  vec3 lambert = Lambert_eval(sd, normalize(wo));
  vec3 lightWeight = DirectionalLight_eval(sunLight);
  float shadow = float(!traceShadowRay(Ray(sd.P, 1000 * wo)));

  float t = abs(dot(sd.P - light.position, normalize(sunLight.direction))) / length(sunLight.direction);
  float T = evalTransmittance(t, MediumSample_mu_t(sampleMedium(sd.P)));

  return T * lambert * lightWeight * shadow;
}

vec3 evalNEEVolume(vec3 P, vec3 wi) {
  Light light = deref(p.scene.lights, 0);
  const DirectionalLight sunLight = DirectionalLight(light.direction, light.color, light.intensity);

  vec3 wo = -normalize(sunLight.direction);
  float phase = HGPhase_eval(HG_FORWARDNESS, dot(wo, wi));
  vec3 lightWeight = DirectionalLight_eval(sunLight);
  float shadow = float(!traceShadowRay(Ray(P, 1000 * wo)));

  return phase * lightWeight * shadow;
}

#define PATH_TRACER_MAX_BOUNCES 5

vec3 tracePathVolume(inout RandomSampler rng, ivec2 pixel, ShadingData sd, vec3 wi) {
  vec3 contribution = vec3(0.0);
  vec3 throughput = vec3(1.0);

  uint depth = 0;
  uint maxDepth = PATH_TRACER_MAX_BOUNCES + 1;

  Ray ray = Ray(p.scene.camera.position, -wi);

  while (true) {
    // Handle volume interaction
    const float tMax = distance(ray.origin, sd.P);

    vec3 mediumPosition = ray.origin;
    float t = 0.f;

    bool terminate = false;
    bool scatter = false;

    // TOOD: For now assume volume is everywhere, no bounding volume
    while (true) {
      MediumSample ms = sampleMedium(mediumPosition);
      float mu_t = MediumSample_mu_t(ms);

      TransmittanceSample ts = sampleTransmittance(rng, mu_t);
      t += ts.t;
      mediumPosition = ray.origin + t * ray.direction;

      // Did we hit a surface?
      if (t < tMax) {
        float pdfAbsorption = ms.mu_a / mu_t;
        float pdfScattering = ms.mu_s / mu_t;

        float rnd = RandomSampler_nextFloat(rng);

        // Handle different volume events
        if (rnd < pdfAbsorption) {
          // Absorption event
          contribution += throughput * ms.emission;
          terminate = true;
          break;
        } else if (pdfAbsorption < rnd && rnd < pdfAbsorption + pdfScattering) {
          // Scattering event

          if (depth++ >= maxDepth) {
            terminate = true;
            break;
          }

          // Evaluate NEE in volume
          contribution += throughput * evalNEEVolume(mediumPosition, wi);

          // Sample phase function
          BRDFSample brdfSample = HGPhase_sample(rng, wi, HG_FORWARDNESS);

          // Update ray
          ray = Ray(mediumPosition, brdfSample.wo);
          Hit hit;
          if(!traceRay(ray, hit)) {
            terminate = true;
            break;
          }

          sd = queryShadingData(p.scene, hit.instanceID, hit.primitiveID, hit.barycentrics);
          wi = -brdfSample.wo;

          scatter = true;

        } else {
          // Null scattering event
        }

      } else {
        break;
      }

      if (terminate)
        break;

      if (scatter)
        continue;
    }



    // Handle surface interaction:

    // NEE - Direct Illumination (analytic lights only)
    vec3 nee = evalNEE(sd, wi);
    contribution += throughput * nee;

    // Emissive surface - randomly hit a light source
    vec3 emission = evalEmissive(sd);
    contribution += throughput * emission;

    if (depth++ >= maxDepth) {
      break;
    }


    // BSDF - Sample outgoing direction
    BRDFSample brdfSample = sampleBRDF(rng, sd, wi);
    throughput *= brdfSample.weight;

    // Trace path
    ray = Ray(sd.P + M_EPSILON * sd.N, brdfSample.wo);
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
  color = tracePathVolume(rng, pixel, sd, wi);
  imageStore(gOutputImage, pixel, vec4(color, 1.0));
}
