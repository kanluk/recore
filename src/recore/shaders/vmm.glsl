#ifndef VMM_GLSL
#define VMM_GLSL

#include "vmm.glslh"

#include "random.glsl"
#include "constants.glsl"
#include "bda.glsl"
#include "math.glsl"



float VMF_pdf(VMF vmf, vec3 direction) {
  float pdf = 0.0;
  if (vmf.kappa > 0) {
    pdf = exp(vmf.kappa * (dot(vmf.mu, direction) - 1.0)) * (vmf.kappa / (2.0 * M_PI * (1 - exp(-2.0 * vmf.kappa))));
  } else {
    pdf = 1.f / (4.0 * M_PI);
  }
  return pdf;
}

vec3 VMF_sample(VMF vmf, inout RandomSampler rng) {
  float xi0 = RandomSampler_nextFloat(rng);
  float xi1 = RandomSampler_nextFloat(rng);

  float W = 1.0 + log(xi0 + (1.0 - xi0) * exp(-2.0 * vmf.kappa)) / vmf.kappa;
  float W2 = sqrt(1 - W * W);

  vec3 direction = vec3(W2 * cos(2.0 * M_PI * xi1), W2 * sin(2.0 * M_PI * xi1), W);
  mat3 onb = tangentFrame(vmf.mu);
  return normalize(onb * direction);
}

void VMF_sample(VMF vmf, inout RandomSampler rng, out DirectionalSample ds) {
  ds.wo = VMF_sample(vmf, rng);
  ds.pdf = VMF_pdf(vmf, ds.wo);
}

float VMF_KL(VMF vmf0, VMF vmf1) {
  float meanCosine = (1.f / tanh(vmf0.kappa)) - (1.f / vmf0.kappa);
  float c0 = vmf0.kappa * sinh(vmf1.kappa);
  float c1 = vmf1.kappa * sinh(vmf0.kappa);
  return log(c0 / c1) + vmf0.kappa * meanCosine - vmf1.kappa * meanCosine * dot(vmf0.mu, vmf1.mu);
}

// float VMF_MD(VMF vmf0, VMF vmf1) {
//   return
// }


VMM VMM_initRandom(inout RandomSampler rng) {
  VMM vmm;
  for (int i = 0; i < VMM_NUM_COMPONENTS; i++) {
    vmm.components[i].vmf.mu = sampleSphere(rng);
    vmm.components[i].vmf.kappa = 10.f;
    vmm.components[i].weight = 1.0 / VMM_NUM_COMPONENTS;
    vmm.components[i].sumWeightedDirections = vec3(0.0);
    vmm.components[i].sumWeights = 0.0;
  }
  vmm.numSamples = 0;
  return vmm;
}

float VMM_pdf(VMM vmm, vec3 direction) {
  float pdf = 0.0;
  for (int i = 0; i < VMM_NUM_COMPONENTS; i++) {
    pdf += vmm.components[i].weight * VMF_pdf(vmm.components[i].vmf, direction);
  }
  return pdf;
}

vec3 VMM_sample(VMM vmm, inout RandomSampler rng) {
  float xi0 = RandomSampler_nextFloat(rng);

  float cdf = 0.0;
  uint i = 0;
  for (; i < VMM_NUM_COMPONENTS - 1; i++) {
    float newCdf = cdf + vmm.components[i].weight;
    if (xi0 < newCdf) {
      break;
    }
    cdf = newCdf;
  }

  return VMF_sample(vmm.components[0].vmf, rng);
}

void VMM_sample(VMM vmm, inout RandomSampler rng, out DirectionalSample ds) {
  ds.wo = VMM_sample(vmm, rng);
  ds.pdf = VMM_pdf(vmm, ds.wo);
}


struct VMM_Sample {
  vec3 direction;
  float weight;
};

// Expectation maximization
void VMM_EM(inout VMM vmm, VMM_Sample new) {
  if (new.weight < M_EPSILON || isinf(new.weight) || isnan(new.weight)) {
    return;
  }

  // E-step: Get soft assignments
  float assignments[VMM_NUM_COMPONENTS];
  float sumAssignments = 0.0;
  for (int i = 0; i < VMM_NUM_COMPONENTS; i++) {
    assignments[i] = vmm.components[i].weight * VMF_pdf(vmm.components[i].vmf, new.direction);
    sumAssignments += assignments[i];
  }
  // Normalize
  for (int i = 0; i < VMM_NUM_COMPONENTS; i++) {
    assignments[i] /= sumAssignments;
  }

  // Update sufficient statistics
  for (int i = 0; i < VMM_NUM_COMPONENTS; i++) {
    vmm.components[i].sumWeightedDirections += assignments[i] * new.direction * new.weight;
    vmm.components[i].sumWeights += assignments[i] * new.weight;
  }

  float totalSumWeights = 0.0;
  for (int i = 0; i < VMM_NUM_COMPONENTS; i++) {
    totalSumWeights += vmm.components[i].sumWeights;
  }

  if (totalSumWeights < M_EPSILON) {
    return;
  }

  vmm.numSamples += 1.f;

  // M-step: Update parameters: weight (pi), mu, kappa
  for (int i = 0; i < VMM_NUM_COMPONENTS; i++) {
    float weight = vmm.components[i].sumWeights / totalSumWeights;
    vec3 mu = normalize(vmm.components[i].sumWeightedDirections);

    float l = length(vmm.components[i].sumWeightedDirections);
    float kappa = l / vmm.components[i].sumWeights;
    kappa = (3.0 * kappa - kappa * kappa * kappa) / (1.0 - kappa * kappa);
    kappa = clamp(kappa, 1.f, 50.f);

    if (ISINF_OR_ISNAN_S(weight) || ISINF_OR_ISNAN(mu)) {
      continue;
    }

    vmm.components[i].weight = weight;
    vmm.components[i].vmf.mu = mu;
    vmm.components[i].vmf.kappa = kappa;
  }
}


bool VMM_isEmpty(VMM vmm) {
  return vmm.numSamples < 0.1f;
}


#endif // VMM_GLSL
