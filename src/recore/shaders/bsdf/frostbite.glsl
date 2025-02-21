#ifndef FROSTBITE_GLSL
#define FROSTBITE_GLSL

// Frostbite standard model
// https://seblagarde.files.wordpress.com/2015/07/course_notes_moving_frostbite_to_pbr_v32.pdf

#define SHADING_EPSILON 0


// Evaluation

vec3 F_Schlick(vec3 f0, float f90, float u) {
  return f0 + (f90 - f0) * pow(1.f - u, 5.f);
}

float V_SmithGGXCorrelated(float NdotL, float NdotV, float alphaG) {
  float alphaG2 = alphaG * alphaG;
  float Lambda_GGXV = NdotL * sqrt((-NdotV * alphaG2 + NdotV) * NdotV + alphaG2);
  float Lambda_GGXL = NdotV * sqrt((-NdotL * alphaG2 + NdotL) * NdotL + alphaG2);
  return 0.5f / (Lambda_GGXV + Lambda_GGXL);
}

float D_GGX(float NdotH, float m) {
  float m2 = m * m;
  float f = (NdotH * m2 - NdotH) * NdotH + 1;
  return m2 / (f * f * M_PI);
}

float Fr_DisneyDiffuse(float NdotV, float NdotL, float LdotH,
float linearRoughness) {
  float energyBias = mix(0, 0.5, linearRoughness);
  float energyFactor = mix(1.0, 1.0 / 1.51, linearRoughness);
  float fd90 = energyBias + 2.0 * LdotH * LdotH * linearRoughness;
  vec3 f0 = vec3(1.0f, 1.0f, 1.0f);
  float lightScatter = F_Schlick(f0, fd90, NdotL).r;
  float viewScatter = F_Schlick(f0, fd90, NdotV).r;
  return lightScatter * viewScatter * energyFactor;
}

vec3 Frostbite_eval(ShadingData sd, vec3 L, vec3 V) {
  // Compute all dot products for shading
  vec3 H = normalize(V + L);
  float NdotV = max(abs(dot(sd.N, V)), 0) + SHADING_EPSILON;
  float NdotH = max(dot(sd.N, H), 0) + SHADING_EPSILON;
  float LdotH = max(dot(L, H), 0) + SHADING_EPSILON;
  float VdotH = max(dot(V, H), 0) + SHADING_EPSILON;
  float NdotL = max(dot(sd.N, L), 0) + SHADING_EPSILON;

  vec3 albedo = mix(sd.albedo, vec3(0.0), sd.metallic);
  vec3 F0 = mix(vec3(0.04), sd.albedo, sd.metallic);
  float roughness2 = sd.roughness * sd.roughness;

  vec3 F = F_Schlick(F0, 1.0, LdotH);
  float G = V_SmithGGXCorrelated(NdotV, NdotL, roughness2);
  float D = D_GGX(NdotH, roughness2);
  vec3 Fr = (D * F * G);

  float Fd = Fr_DisneyDiffuse(NdotV, NdotL, LdotH, sd.roughness);

  return Fd * albedo / M_PI + Fr;
}


// Sampling

vec3 sampleGGXNDF(inout RandomSampler rng, float roughness) {
  float xi0 = RandomSampler_nextFloat(rng);
  float xi1 = RandomSampler_nextFloat(rng);

  float alphaSqr = roughness * roughness;
  float phi = xi1 * 2 * M_PI;
  float tanThetaSqr = alphaSqr * xi0 / (1 - xi0);
  float cosTheta = 1 / sqrt(1 + tanThetaSqr);
  float r = sqrt(max(1 - cosTheta * cosTheta, 0));

  return vec3(cos(phi) * r, sin(phi) * r, cosTheta);
}

float pdfGGXNDF(vec3 H, float roughness) {
  return D_GGX(H.z, roughness + SHADING_EPSILON) * max(0, H.z);
}



// https://jcgt.org/published/0007/04/01/paper.pdf
vec3 sampleGGXVNDF(inout RandomSampler rng, vec3 Ve, float roughness) {
  float xi0 = RandomSampler_nextFloat(rng);
  float xi1 = RandomSampler_nextFloat(rng);

  // Section 3.2: transforming the view direction to the hemisphere configuration
  vec3 Vh = normalize(vec3(roughness * Ve.x, roughness * Ve.y, Ve.z));
  // Section 4.1: orthonormal basis (with special case if cross product is zero)
  float lensq = Vh.x * Vh.x + Vh.y * Vh.y;
  vec3 T1 = lensq > 0 ? vec3(-Vh.y, Vh.x, 0) * inversesqrt(lensq) : vec3(1, 0, 0);
  vec3 T2 = cross(Vh, T1);
  // Section 4.2: parameterization of the projected area
  float r = sqrt(xi0);
  float phi = 2.0 * M_PI * xi1;
  float t1 = r * cos(phi);
  float t2 = r * sin(phi);
  float s = 0.5 * (1.0 + Vh.z);
  t2 = (1.0 - s)*sqrt(1.0 - t1*t1) + s*t2;
  // Section 4.3: reprojection onto hemisphere
  vec3 Nh = t1*T1 + t2*T2 + sqrt(max(0.0, 1.0 - t1*t1 - t2*t2))*Vh;
  // Section 3.4: transforming the normal back to the ellipsoid configuration
  vec3 Ne = normalize(vec3(roughness * Nh.x, roughness * Nh.y, max(0.0, Nh.z)));
  return Ne;
}

// https://www.cs.cornell.edu/~srm/publications/EGSR07-btdf.pdf
float evalG1GGX(float alphaSqr, float cosTheta) {
  if (cosTheta <= 0) return 0;
  float cosThetaSqr = cosTheta * cosTheta;
  float tanThetaSqr = max(1 - cosThetaSqr, 0) / cosThetaSqr;
  return 2 / (1 + sqrt(1 + alphaSqr * tanThetaSqr));
}

float pdfGGXVNDF(vec3 H, vec3 V, float roughness) {
  float VdotH = max(0, dot(V, H));
  float roughness2 = roughness * roughness;

  float G1 = evalG1GGX(roughness2, V.z);
  float D = pdfGGXNDF(H, roughness);
  return G1 * D * VdotH / V.z;
}

float pdfReflection(float VdotH) {
  return 1.0 / (4.0 * (max(0, VdotH) + SHADING_EPSILON));
}

// Sample GGXVNDF with spherical caps
// https://diglib.eg.org/bitstream/handle/10.1111/cgf14867/v42i8_03_14867.pdf
vec3 sampleGGXVNDFSC(inout RandomSampler rng, vec3 Ve, float roughness) {
  float xi0 = RandomSampler_nextFloat(rng);
  float xi1 = RandomSampler_nextFloat(rng);

  vec3 Vh = normalize(vec3(roughness * Ve.x, roughness * Ve.y, Ve.z));

  // sample a spherical cap in (-wi.z, 1]
  float phi = 2.0f * M_PI * xi0;
  float z = fma((1.0f - xi1), (1.0f + Vh.z), -Vh.z);
  float sinTheta = sqrt(clamp(1.0f - z * z, 0.0f, 1.0f));
  float x = sinTheta * cos(phi);
  float y = sinTheta * sin(phi);
  vec3 c = vec3(x, y, z);
  // compute halfway direction;
  vec3 Nh = c + Vh;
  // return without normalization (as this is done later)

  vec3 Ne = normalize(vec3(roughness * Nh.x, roughness * Nh.y, max(0.0, Nh.z)));

  return Ne;
}

DirectionalSample Frostbite_sample(inout RandomSampler rng, ShadingData sd, vec3 wi) {
  DirectionalSample ds;
  vec3 H = sampleGGXVNDFSC(rng, wi, sd.roughness);
  ds.wo = reflect(-wi, H);
  ds.pdf = pdfGGXVNDF(H, wi, sd.roughness) * pdfReflection(dot(wi, H));

  return ds;
}

#endif // FROSTBITE_GLSL
