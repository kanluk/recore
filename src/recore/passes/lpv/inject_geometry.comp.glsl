#version 460

#include "lpv.glslh"
#include "lpv.glsl"


layout(local_size_x = 32, local_size_y = 1, local_size_z = 1) in;

layout(set = 0, binding = 0) uniform sampler2D gOccluderPosition;
layout(set = 0, binding = 0) uniform sampler2D gOccluderNormal;

PUSH_CONSTANT(InjectPush);

void main() {
  ivec2 pixel = ivec2(gl_GlobalInvocationID.xy);
  ivec2 resolution = textureSize(gOccluderPosition, 0);

  if (any(greaterThan(pixel, resolution))) {
    return;
  }

  vec4 positionW = texelFetch(gOccluderPosition, pixel, 0);
  vec3 position = positionW.xyz;
  float depth = positionW.w;
  vec3 normal = texelFetch(gOccluderNormal, pixel, 0).xyz;

  if (length(normal) < 0.01) {
    return;
  }

  vec3 lpvMin = p.lpvGrid.origin;
  vec3 lpvMax = p.lpvGrid.origin + p.lpvGrid.cellSize * p.lpvGrid.dimensions;
  if (any(lessThan(position, lpvMin)) || any(greaterThan(position, lpvMax))) {
    return;
  }


  // Inject occlusion
  uvec3 occlusionGridPosition = LPVGrid_getPositionOcclusion(p.lpvGrid, position);
  uint occlusionGridIndex = LPVGrid_getIndex(p.lpvGrid, occlusionGridPosition);

  // position.w stores the distance between shadow map origin an surface
  float surfelArea = 4.0 * depth * depth / (resolution.x * resolution.y);
  float areaRatio = clamp(surfelArea / (p.lpvGrid.cellSize * p.lpvGrid.cellSize), 0.0, 1.0);

  #ifdef SH_USE_VEC4
  vec4 shNormal = SH_evalCos(normal) / M_PI;
  vec4 shGeometry = shNormal * areaRatio;
  shGeometry = shNormal;

  atomicAddVec4(deref(p.lpvGrid.geometryVolume, occlusionGridIndex).shGeometry, shGeometry);
  atomicAdd(deref(p.lpvGrid.geometryVolume, occlusionGridIndex).counter, 1);

  #else

  SHFunction shNormal = SHFunction_evalCos(normal);
  SHFunction_mul(shNormal, M_1_PI);

  for (uint i = 0; i < SH_NUM_COEFFICIENTS; i++) {
    atomicAdd(deref(p.lpvGrid.geometryVolume, occlusionGridIndex).shGV[i], shNormal[i]);
  }
  atomicAdd(deref(p.lpvGrid.geometryVolume, occlusionGridIndex).counter, 1);

  #endif

}
