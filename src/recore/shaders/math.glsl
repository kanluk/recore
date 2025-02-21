#ifndef MATH_GLSL
#define MATH_GLSL

#define ISINF_OR_ISNAN_S(x) (isinf(x) || isnan(x))
#define ISINF_OR_ISNAN(x) (any(isinf(x)) || any(isnan(x)))

#include "constants.glsl"

float average(vec3 v) {
  return (v.r + v.g + v.b) / 3.0;
}


// Generate local tangent frame
mat3 tangentFrame(vec3 N) {
  vec3 up = abs(N.z) < 0.999 ? vec3(0, 0, 1) : vec3(1, 0, 0);
  vec3 T = normalize(cross(up, N));
  vec3 B = cross(N, T);
  return mat3(T, B, N);
}

vec3 slerp(vec3 v0, vec3 v1, float t) {
  float cosTheta = dot(v0, v1);
  float theta = acos(cosTheta);
  return (sin((1 - t) * theta) * v0 + sin(t * theta) * v1) / sin(theta);
}

float map(float value, float min1, float max1, float min2, float max2) {
  return min2 + (value - min1) * (max2 - min2) / (max1 - min1);
}

float safe_acos(float cosTheta) {
  return abs(cosTheta - 1.f) < M_EPSILON ? 0.f : acos(cosTheta);
}

#endif // MATH_GLSL
