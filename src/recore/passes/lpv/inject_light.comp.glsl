#version 460


#include "lpv.glslh"
#include "lpv.glsl"

layout(local_size_x = 32, local_size_y = 1, local_size_z = 1) in;

layout(set = 0, binding = 0) uniform sampler2D gRSMPosition;
layout(set = 0, binding = 1) uniform sampler2D gRSMNormal;
layout(set = 0, binding = 2) uniform sampler2D gRSMFlux;

PUSH_CONSTANT(InjectPush);

#if 0

float luminance(vec3 c) {
  return (0.2126*c.r + 0.7152*c.g + 0.0722*c.b);
}

void main() {
  ivec2 pixel = ivec2(gl_GlobalInvocationID.xy);
  ivec2 resolution = textureSize(gRSMPosition, 0) / 2;

  if (any(greaterThan(pixel, resolution))) {
    return;
  }

  // Choose brightest texel
  uvec3 gridPosition = uvec3(0);
  vec3 gridPositionF = vec3(0.f);
  float maxLum = 0.f;
  for (int i = 0; i < 2; i++) {
    for (int j = 0; j < 2; j++) {
      ivec2 currPixel = ivec2(pixel.x * 2 + i, pixel.y * 2 + j);
      vec3 position = texelFetch(gRSMPosition, currPixel, 0).xyz;
      float lum = luminance(texelFetch(gRSMFlux, currPixel, 0).xyz);
      if (lum > maxLum) {
        maxLum = lum;
        gridPosition = LPVGrid_getPosition(p.lpvGrid, position);
        gridPositionF = position;
      }
    }
  }

  // Filter

  vec3 fNormal = vec3(0.f);
  vec3 fFlux = vec3(0.f);
  uint fNumSamples = 0;
  for (int i = 0; i < 2; i++) {
    for (int j = 0; j < 2; j++) {
      ivec2 currPixel = ivec2(pixel.x * 2 + i, pixel.y * 2 + j);
      vec3 position = texelFetch(gRSMPosition, currPixel, 0).xyz;
      vec3 normal = texelFetch(gRSMNormal, currPixel, 0).xyz;
      vec3 flux = texelFetch(gRSMFlux, currPixel, 0).xyz;
      uvec3 currGridPosition = LPVGrid_getPosition(p.lpvGrid, position);
      uvec3 dGrid = gridPosition - currGridPosition;
      if (dot(dGrid, dGrid) < 3) {
        fNormal += normal;
        fFlux += flux;
        fNumSamples++;
      }
    }
  }

  fNormal = normalize(fNormal);
  fFlux /= float(fNumSamples);

  vec3 lpvMin = p.lpvGrid.origin;
  vec3 lpvMax = p.lpvGrid.origin + p.lpvGrid.cellSize * p.lpvGrid.dimensions;
  if (any(lessThan(gridPositionF, lpvMin)) || any(greaterThan(gridPositionF, lpvMax))) {
    return;
  }

  // Inject flux

  uint gridIndex = LPVGrid_getIndex(p.lpvGrid, gridPosition);

  vec4 shNormal = SH_evalCos(fNormal) / M_PI;

  vec4 shRed = shNormal * fFlux.r;
  vec4 shGreen = shNormal * fFlux.g;
  vec4 shBlue = shNormal * fFlux.b;

  atomicAddVec4(deref(p.lpvGrid.volume, gridIndex).shRed, shRed);
  atomicAddVec4(deref(p.lpvGrid.volume, gridIndex).shGreen, shGreen);
  atomicAddVec4(deref(p.lpvGrid.volume, gridIndex).shBlue, shBlue);
  atomicAdd(deref(p.lpvGrid.volume, gridIndex).counter, 1);
}

#else

void main() {
  ivec2 pixel = ivec2(gl_GlobalInvocationID.xy);
  ivec2 resolution = textureSize(gRSMPosition, 0);

  if (any(greaterThan(pixel, resolution))) {
    return;
  }

  vec4 positionW = texelFetch(gRSMPosition, pixel, 0);
  vec3 position = positionW.xyz;
  float depth = positionW.w;
  vec3 normal = texelFetch(gRSMNormal, pixel, 0).xyz;
  vec3 flux = texelFetch(gRSMFlux, pixel, 0).xyz;

  if (length(normal) < 0.01) {
    return;
  }
  if (length(flux) < 0.01) {
    return;
  }


  vec3 lpvMin = p.lpvGrid.origin;
  vec3 lpvMax = p.lpvGrid.origin + p.lpvGrid.cellSize * p.lpvGrid.dimensions;
  if (any(lessThan(position, lpvMin)) || any(greaterThan(position, lpvMax))) {
    return;
  }

  // Inject flux

  uvec3 gridPosition = LPVGrid_getPosition(p.lpvGrid, position, normal);
  // uvec3 gridPosition = LPVGrid_getPosition(p.lpvGrid, position);
  uint gridIndex = LPVGrid_getIndex(p.lpvGrid, gridPosition);

#ifdef SH_USE_VEC4
  // Divide by M_PI according to blog (mistake in the paper)
  // float surfelWeight = float(p.lpvGrid.dimensions.x * p.lpvGrid.dimensions.y * p.lpvGrid.dimensions.z) / float(resolution.x * resolution.y);
  // vec4 shNormal = surfelWeight * SH_evalCos(normal) / M_PI;
  vec4 shNormal = SH_evalCos(normal) / M_PI;
  // shNormal = SH_evalCos(-normal);
  // shNormal.x = 0.01f;

#if 1
  vec4 shRed = shNormal * flux.r;
  vec4 shGreen = shNormal * flux.g;
  vec4 shBlue = shNormal * flux.b;
#else
  vec4 shRed = shNormal;
  vec4 shGreen = shRed;
  vec4 shBlue = shRed;
#endif

  atomicAddVec4(deref(p.lpvGrid.volume, gridIndex).shRed, shRed);
  atomicAddVec4(deref(p.lpvGrid.volume, gridIndex).shGreen, shGreen);
  atomicAddVec4(deref(p.lpvGrid.volume, gridIndex).shBlue, shBlue);
  atomicAdd(deref(p.lpvGrid.volume, gridIndex).counter, 1);
#else

  SHFunction shNormal = SHFunction_evalCos(normal);
  SHFunction_mul(shNormal, M_1_PI);
  SHRGBFunction shRGB = SHRGBFunction_eval(shNormal, flux);

  for (uint i = 0; i < SH_NUM_COEFFICIENTS; i++) {
    atomicAdd(deref(p.lpvGrid.volume, gridIndex).shRGB.r[i], shRGB.r[i]);
    atomicAdd(deref(p.lpvGrid.volume, gridIndex).shRGB.g[i], shRGB.g[i]);
    atomicAdd(deref(p.lpvGrid.volume, gridIndex).shRGB.b[i], shRGB.b[i]);
  }
  atomicAdd(deref(p.lpvGrid.volume, gridIndex).counter, 1);


#endif

}

#endif
