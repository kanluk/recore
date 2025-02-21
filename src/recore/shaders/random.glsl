#ifndef RANDOM_GLSL
#define RANDOM_GLSL

#include "constants.glsl"

// Shader RNG from: https://nvpro-samples.github.io/vk_mini_path_tracer/index.html

struct RandomSampler {
  uint state;
};

RandomSampler RandomSampler_init(uint index, uint frameCount) {
  return RandomSampler(index * (frameCount + 1));
}

RandomSampler RandomSampler_init(ivec2 pixel, ivec2 resolution) {
  return RandomSampler(resolution.x * pixel.y + pixel.x);
}

RandomSampler RandomSampler_init(ivec2 pixel, ivec2 resolution, uint frameCount) {
  return RandomSampler((resolution.x * pixel.y + pixel.x) * (frameCount + 1));
}

RandomSampler RandomSampler_init(ivec2 pixel, ivec2 resolution, uint seed, uint frameCount) {
  return RandomSampler((resolution.x * pixel.y + pixel.x) * (frameCount + 1) + seed);
}

// Random number generation using pcg32i_random_t, using inc = 1. Our random state is a uint.
uint stepRNG(uint rngState) {
  return rngState * 747796405 + 1;
}

// Steps the RNG and returns a floating-point value between 0 and 1 inclusive.
float RandomSampler_nextFloat(inout RandomSampler rng) {
  // Condensed version of pcg_output_rxs_m_xs_32_32, with simple conversion to floating-point [0,1].
  rng.state  = stepRNG(rng.state);
  uint word = ((rng.state >> ((rng.state >> 28) + 4)) ^ rng.state) * 277803737;
  word      = (word >> 22) ^ word;
  return float(word) / 4294967295.0f;
}

vec2 RandomSampler_nextVec2(inout RandomSampler rng) {
  return vec2(RandomSampler_nextFloat(rng), RandomSampler_nextFloat(rng));
}


struct DirectionalSample {
  vec3 wo;
  float pdf;
};

// ===========================================
// Sampling Sphere / Hemisphere
// ===========================================


float pdfSampleSphere() {
  return 1.0 / (4.0 * M_PI);
}

vec3 sampleSphere(inout RandomSampler rng) {
  float xi0 = RandomSampler_nextFloat(rng);
  float xi1 = RandomSampler_nextFloat(rng);

  float theta = 2 * M_PI * xi0;
  float z = 2 * xi1 - 1;
  float r = sqrt(1 - z * z);
  return vec3(r * cos(theta), r * sin(theta), z);
}

void sampleSphere(inout RandomSampler rng, out DirectionalSample ds) {
  ds.wo = sampleSphere(rng);
  ds.pdf = pdfSampleSphere();
}


float pdfSampleHemisphere() {
  return 1.0 / (2.0 * M_PI);
}

vec3 sampleHemisphere(inout RandomSampler rng) {
  float xi1 = RandomSampler_nextFloat(rng);
  float xi2 = RandomSampler_nextFloat(rng);

  float cosTheta = xi1;
  float phi = 2.0 * M_PI * xi2;
  float sinTheta = sqrt(1 - cosTheta * cosTheta);

  return vec3(sinTheta * cos(phi), sinTheta * sin(phi), cosTheta);
}

void sampleHemisphere(inout RandomSampler rng, out DirectionalSample ds) {
  ds.wo = sampleHemisphere(rng);
  ds.pdf = pdfSampleHemisphere();
}


float pdfSampleCosineHemisphere(float cosTheta) {
  return cosTheta / M_PI;
}

vec3 sampleCosineHemisphere(inout RandomSampler rng) {
  float xi1 = RandomSampler_nextFloat(rng);
  float xi2 = RandomSampler_nextFloat(rng);

  float cosTheta = sqrt(xi1);
  float phi = 2.0 * M_PI * xi2;
  float sinTheta = sqrt(1 - cosTheta * cosTheta);

  return vec3(sinTheta * cos(phi), sinTheta * sin(phi), cosTheta);
}

void sampleCosineHemisphere(inout RandomSampler rng, out DirectionalSample ds) {
  ds.wo = sampleCosineHemisphere(rng);
  ds.pdf = pdfSampleCosineHemisphere(ds.wo.z);
}


float pdfSamplePowerCosineHemisphere(float cosTheta, float power) {
  return (power + 1) * pow(cosTheta, power) / (2.0 * M_PI);
}

vec3 samplePowerCosineHemisphere(inout RandomSampler rng, float power) {
  float xi1 = RandomSampler_nextFloat(rng);
  float xi2 = RandomSampler_nextFloat(rng);

  float cosTheta = pow(xi1, 1.0 / (power + 1));
  float phi = 2.0 * M_PI * xi2;
  float sinTheta = sqrt(1 - cosTheta * cosTheta);

  return vec3(sinTheta * cos(phi), sinTheta * sin(phi), cosTheta);
}

void samplePowerCosineHemisphere(inout RandomSampler rng, out DirectionalSample ds, float power) {
  ds.wo = samplePowerCosineHemisphere(rng, power);
  ds.pdf = pdfSamplePowerCosineHemisphere(ds.wo.z, power);
}

#endif // RANDOM_GLSL
