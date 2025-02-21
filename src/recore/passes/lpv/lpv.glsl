#ifndef LPV_GLSL
#define LPV_GLSL

#extension GL_EXT_shader_atomic_float : require
#extension GL_EXT_control_flow_attributes : require

#include "lpv.glslh"

#include <recore/shaders/constants.glsl>


uvec3 LPVGrid_getPosition(LPVGrid grid, vec3 position) {
  return uvec3((position - grid.origin) / grid.cellSize);
}

// Shift into normal direction (to avoid self intersection I guess?)
uvec3 LPVGrid_getPosition(LPVGrid grid, vec3 position, vec3 normal) {
  // return uvec3((position - grid.origin) / grid.cellSize + 0.f * normal);
  return uvec3((position - grid.origin) / grid.cellSize + 0.5 * normal);
}

// Shift by half a cell
uvec3 LPVGrid_getPositionOcclusion(LPVGrid grid, vec3 position) {
  return uvec3((position - grid.origin) / grid.cellSize - vec3(0.5, 0.5, 0.5));
}


uint LPVGrid_getIndex(LPVGrid grid, uvec3 position) {
  return position.x + position.y * grid.dimensions.x + position.z * grid.dimensions.x * grid.dimensions.y;
}



#define SH_C0 0.282094792f // 1 / (2 * sqrt(pi))
#define SH_C1 0.488602512f // sqrt(3 / pi) / 2
#define SH_BASE vec4(SH_C0, SH_C1, SH_C1, SH_C1)

#define SH_C0_COS 0.886226925f // sqrt(pi) / 2
#define SH_C1_COS 1.02332671f // sqrt(pi / 3)
#define SH_BASE_COS vec4(SH_C0_COS, SH_C1_COS, SH_C1_COS, SH_C1_COS)


// from https://en.wikipedia.org/wiki/Table_of_spherical_harmonics#Real_spherical_harmonics
// and https://github.com/google/spherical-harmonics/blob/master/sh/spherical_harmonics.cc
// TODO: signs shouldn't matter for a basis, idk why they choose - for some coefficients
const float SH_COEFFICIENTS[] = {
  // l = 0
  0.282095f,
  // l = 1
  0.488603f,
  0.488603f,
  0.488603f,

  // l = 2
  1.092548f,
  1.092548f,
  0.315392f,
  1.092548f,
  0.546274f,

  // l = 3
  0.590044f,
  2.890611f,
  0.457046f,
  0.373176f,
  0.457046f,
  1.445306f,
  0.590044f,

};

const float SH_COEFFICIENTS_COSINE[] = {
  // l = 0
  3.141593f,

  // l = 1
  2.094395f,
  2.094395f,
  2.094395f,

  // l = 2
  0.785398f,
  0.785398f,
  0.785398f,
  0.785398f,
  0.785398f,

  // l = 3
  0.0f,
  0.0f,
  0.0f,
  0.0f,
  0.0f,
  0.0f,
  0.0f,
};

vec4 SH_eval(vec3 direction) {
  return SH_BASE * vec4(1.0, direction.yzx);
}

vec4 SH_evalCos(vec3 direction) {
  return SH_BASE_COS * vec4(1.0, direction.yzx);
}


SHFunction SHFunction_eval(vec3 direction) {
  SHFunction sh;

  float x = direction.x;
  float y = direction.y;
  float z = direction.z;

  const float poly[] = {
  #if SH_NUM_BANDS >= 1
    // l = 0
    1.f,
  #endif
  #if SH_NUM_BANDS >= 2
    // l = 1
    y,
    z,
    x,
  #endif
  #if SH_NUM_BANDS >= 3
    // l = 2
    x * y,
    y * z,
    (3 * z * z - 1),
    x * z,
    x * x - y * y,
  #endif
  #if SH_NUM_BANDS >= 4
    // l = 3
    y * (3 * x * x - y * y),
    x * y * z,
    y * (5 * z * z - 1),
    5 * z * z * z - 3 * z,
    x * (5 * z * z - 1),
    (x * x - y * y) * z,
    x * (x * x - 3 * y * y),
  #endif

  #if SH_NUM_BANDS > 4
  #error MISSING COEFFICIENTS!
  #endif
  };

  [[unroll]]
  for (uint i = 0; i < SH_NUM_COEFFICIENTS; i++) {
    sh[i] = SH_COEFFICIENTS[i] * poly[i];
  }

  return sh;
}

SHFunction SHFunction_evalCos(vec3 direction) {
  SHFunction sh = SHFunction_eval(direction);

  [[unroll]]
  for (uint i = 0; i < SH_NUM_COEFFICIENTS; i++) {
    sh[i] *= SH_COEFFICIENTS_COSINE[i];
  }
  return sh;
}

void SHFunction_add(inout SHFunction sh, SHFunction value) {
  [[unroll]]
  for (uint i = 0; i < SH_NUM_COEFFICIENTS; i++) {
  sh[i] += value[i];
  }
}

void SHFunction_mul(inout SHFunction sh, float value) {
  [[unroll]]
  for (uint i = 0; i < SH_NUM_COEFFICIENTS; i++) {
    sh[i] *= value;
  }
}

void SHFunction_init(inout SHFunction sh) {
  [[unroll]]
  for (uint i = 0; i < SH_NUM_COEFFICIENTS; i++) {
    sh[i] = 0.0f;
  }
}

float SHFunction_dot(SHFunction sh1, SHFunction sh2) {
  float sum = 0.0f;

  [[unroll]]
  for (uint i = 0; i < SH_NUM_COEFFICIENTS; i++) {
    sum += sh1[i] * sh2[i];
  }

  return sum;
}

void SHFunction_mad(inout SHFunction sh0, SHFunction sh1, float value) {
  [[unroll]]
  for (uint i = 0; i < SH_NUM_COEFFICIENTS; i++) {
    sh0[i] += sh1[i] * value;
  }
}

SHRGBFunction SHRGBFunction_init() {
  SHRGBFunction shRGB;

  SHFunction_init(shRGB.r);
  SHFunction_init(shRGB.g);
  SHFunction_init(shRGB.b);

  return shRGB;
}

SHRGBFunction SHRGBFunction_eval(SHFunction shFunction, vec3 value) {
  SHRGBFunction shRGB;
  shRGB.r = shFunction;
  shRGB.g = shFunction;
  shRGB.b = shFunction;

  [[unroll]]
  for (uint i = 0; i < SH_NUM_COEFFICIENTS; i++) {
    shRGB.r[i] *= value.r;
    shRGB.g[i] *= value.g;
    shRGB.b[i] *= value.b;
  }
  return shRGB;
}

void SHRGBFunction_add(inout SHRGBFunction shRGB, SHRGBFunction value) {
  SHFunction_add(shRGB.r, value.r);
  SHFunction_add(shRGB.g, value.g);
  SHFunction_add(shRGB.b, value.b);
}

void SHRGBFunction_mul(inout SHRGBFunction shRGB, float value) {
  SHFunction_mul(shRGB.r, value);
  SHFunction_mul(shRGB.g, value);
  SHFunction_mul(shRGB.b, value);
}

void SHRGBFunction_mul(inout SHRGBFunction shRGB, vec3 value) {
  SHFunction_mul(shRGB.r, value.r);
  SHFunction_mul(shRGB.g, value.g);
  SHFunction_mul(shRGB.b, value.b);
}

vec3 SHRGBFunction_dot(SHRGBFunction shRGB, SHFunction sh) {
  return vec3(
    SHFunction_dot(shRGB.r, sh),
    SHFunction_dot(shRGB.g, sh),
    SHFunction_dot(shRGB.b, sh)
  );
}

SHFunction SHRGBFunction_max(SHRGBFunction shRGB) {
  SHFunction shMax;

  [[unroll]]
  for (uint i = 0; i < SH_NUM_COEFFICIENTS; i++) {
    shMax[i] = max(shRGB.r[i], max(shRGB.g[i], shRGB.b[i]));
  }
  return shMax;
}

void SHRGBFunction_mad(inout SHRGBFunction shRGB, SHFunction sh, vec3 value) {
  SHFunction_mad(shRGB.r, sh, value.r);
  SHFunction_mad(shRGB.g, sh, value.g);
  SHFunction_mad(shRGB.b, sh, value.b);
}

void SHRGBFunction_mad(inout SHRGBFunction shRGB0, SHRGBFunction shRGB1, float value) {
  SHFunction_mad(shRGB0.r, shRGB1.r, value);
  SHFunction_mad(shRGB0.g, shRGB1.g, value);
  SHFunction_mad(shRGB0.b, shRGB1.b, value);
}

#define atomicAddVec4(address, value) \
  atomicAdd(address.x, value.x); \
  atomicAdd(address.y, value.y); \
  atomicAdd(address.z, value.z); \
  atomicAdd(address.w, value.w);




RadianceVolumeCell LPVGrid_sampleNearestDirect(LPVGrid grid, vec3 position) {
  uvec3 base = LPVGrid_getPosition(grid, position);
  uint index = LPVGrid_getIndex(grid, base);
  return deref(grid.volume, index);
}

RadianceVolumeCell LPVGrid_sampleNearestDirect(LPVGrid grid, vec3 position, vec3 normal) {
  uvec3 base = LPVGrid_getPosition(grid, position, normal);
  uint index = LPVGrid_getIndex(grid, base);
  return deref(grid.volume, index);
}

RadianceVolumeCell LPVGrid_sampleNearest(LPVGrid grid, vec3 position) {
  uvec3 base = LPVGrid_getPosition(grid, position);
  uint index = LPVGrid_getIndex(grid, base);
  return deref(grid.volumeAccu, index);
}

RadianceVolumeCell LPVGrid_sampleTrilinear(LPVGrid grid, vec3 position) {
  RadianceVolumeCell cell;

  #ifdef SH_USE_VEC4
  cell.shRed = vec4(0);
  cell.shGreen = vec4(0);
  cell.shBlue = vec4(0);
  #else
  cell.shRGB = SHRGBFunction_init();
  #endif

  vec3 gridPosition = (position - grid.origin) / grid.cellSize;
  uvec3 base = LPVGrid_getPosition(grid, position);

  float x = fract(gridPosition.x);
  float y = fract(gridPosition.y);
  float z = fract(gridPosition.z);

  const float w[8] = {
        (1 - x) * (1 - y) * (1 - z),    x * (1 - y) * (1 - z),  (1 - x) * y * (1 - z),  x * y * (1 - z),
        (1 - x) * (1 - y) * z,          x * (1 - y) * z,        (1 - x) * y * z,        x * y * z
    };

  const uvec3 offset[8] = {
      uvec3(0, 0, 0), uvec3(1, 0, 0), uvec3(0, 1, 0), uvec3(1, 1, 0),
      uvec3(0, 0, 1), uvec3(1, 0, 1), uvec3(0, 1, 1), uvec3(1, 1, 1)
  };

  float sumw = 0.0;
  for (uint i = 0; i < 8; i++) {
    uvec3 tapGridPosition = base + offset[i];
    uint tapIndex = LPVGrid_getIndex(grid, tapGridPosition);
    RadianceVolumeCell tapCell = deref(grid.volumeAccu, tapIndex);

    if (tapCell.counter == 0) {
      continue;
    }

    #ifdef SH_USE_VEC4
    cell.shRed += w[i] * tapCell.shRed;
    cell.shGreen += w[i] * tapCell.shGreen;
    cell.shBlue += w[i] * tapCell.shBlue;
    #else
    SHRGBFunction_mad(cell.shRGB, tapCell.shRGB, w[i]);
    #endif
    sumw += w[i];
  }

  if (sumw < 0.0001) {
    return cell;
  }

  #ifdef SH_USE_VEC4
  cell.shRed /= sumw;
  cell.shGreen /= sumw;
  cell.shBlue /= sumw;
  #else

  #endif

  cell.counter = 1;

  return cell;
}




#endif // LPV_GLSL
