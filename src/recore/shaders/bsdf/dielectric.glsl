#ifndef DIELECTRIC_GLSL
#define DIELECTRIC_GLSL

// Dielectric material
// Code inspired from the image synthesis exercies in Nori

float evalFresnelDielectric(float cos_i, float eta1, float eta2) {
  if (eta1 > eta2) {
    float sin_total_internal_reflection = eta2 / eta1;
    float sin_i = sqrt(1.f - cos_i * cos_i);
    if (sin_i > sin_total_internal_reflection) return 1.f;
  }

  float eta_ratio = eta1 / eta2;

  float cos_t = sqrt(1.f - eta_ratio * eta_ratio * (1.f - cos_i * cos_i));
  float rs = (eta1 * cos_i - eta2 * cos_t) / (eta1 * cos_i + eta2 * cos_t);
  rs = rs * rs;

  float rp = (eta1 * cos_t - eta2 * cos_i) / (eta1 * cos_t + eta2 * cos_i);
  rp = rp * rp;

  return (rs + rp) / 2.f;
}

vec3 transmitDielectric(vec3 V_t, float eta1, float eta2) {
  vec3 N = V_t.z < 0 ? vec3(0, 0, -1) : vec3(0, 0, 1);
  const float eta_ratio = eta1 / eta2;
  float cos_i = dot(V_t, N);
  float cos_t = sqrt(1.f - eta_ratio * eta_ratio * (1.f - cos_i * cos_i));
  return -eta_ratio * V_t + (eta_ratio * cos_i - cos_t) * N;
}


vec3 Dielectric_sample(inout RandomSampler rng, vec3 wi, float eta) {
  float eta1 = wi.z < 0 ? eta : 1.0;
  float eta2 = wi.z < 0 ? 1.0 : eta;

  float F = evalFresnelDielectric(abs(wi.z), eta1, eta2);
  float xi0 = RandomSampler_nextFloat(rng);

  vec3 randomDirection = vec3(0.f);
  if (xi0 < F) {
    // Reflection
    randomDirection = vec3(-wi.x, -wi.y, wi.z);
  } else {
    // Transmission
    randomDirection = transmitDielectric(wi, eta1, eta2);
  }


  return randomDirection;
}


#endif // DIELECTRIC_GLSL
