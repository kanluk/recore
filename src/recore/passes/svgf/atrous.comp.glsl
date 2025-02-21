#version 460

#include "atrous.glslh"


layout(local_size_x = 32, local_size_y = 32, local_size_z = 1) in;

// Output
layout(set = 0, binding = 0, rgba32f) uniform writeonly image2D gFilteredImage;

// Input
layout(set = 0, binding = 1) uniform sampler2D gPosition;
layout(set = 0, binding = 2) uniform sampler2D gNormal;
layout(set = 0, binding = 3) uniform sampler2D gColor;

PUSH_CONSTANT(ATrousPush);

#define FILTER_SIZE 5

// Gaussian kernel matrices for 3x3 and 5x5
#if FILTER_SIZE == 3
const float kernel[9] = float[](
1.0/16.0, 1.0/8.0, 1.0/16.0,
1.0/8.0, 1.0/4.0, 1.0/8.0,
1.0/16.0, 1.0/8.0, 1.0/16.0
);
#else
const float kernel[25] = float[](
0.003, 0.013, 0.022, 0.013, 0.003,
0.013, 0.059, 0.097, 0.059, 0.013,
0.022, 0.097, 0.159, 0.097, 0.022,
0.013, 0.059, 0.097, 0.059, 0.013,
0.003, 0.013, 0.022, 0.013, 0.003
);
#endif

void main() {
  ivec2 pixel = ivec2(gl_GlobalInvocationID.xy);
  ivec2 resolution = imageSize(gFilteredImage);

  if (any(greaterThanEqual(pixel, resolution))) {
    return;
  }

  // Load current pixel data
  vec4 positionW = texelFetch(gPosition, pixel, 0);
  vec3 position = positionW.xyz;
  vec3 normal = normalize(texelFetch(gNormal, pixel, 0).rgb);
  vec4 color = texelFetch(gColor, pixel, 0);

  if (positionW.w < 1.f) {
    // do not filter the void
    imageStore(gFilteredImage, pixel, color);
    return;
  }

  vec4 sum = vec4(0.f);
  float sumw = 0.f;

  for (int y = 0; y < FILTER_SIZE; y++) {
    for (int x = 0; x < FILTER_SIZE; x++) {
      ivec2 offset = ivec2(x, y) - (FILTER_SIZE/2);
      ivec2 uv = pixel + offset * int(p.stepWidth);

      if (all(greaterThanEqual(uv, ivec2(0, 0))) && all(lessThan(uv, resolution))) {
        vec4 ctmp = texelFetch(gColor, uv, 0);
        vec4 cT = color - ctmp;
        float dist2 = dot(cT, cT);
        float c_w = min(exp(-(dist2)/p.phiColor), 1.0);

        vec3 ntmp = texelFetch(gNormal, uv, 0).xyz;
        vec3 nT = normal - ntmp;
        dist2 = dot(nT, nT);
        float n_w = min(exp(-(dist2)/p.phiNormal), 1.0);

        vec3 ptmp = texelFetch(gPosition, uv, 0).xyz;
        vec3 pT = position - ptmp;
        dist2 = dot(pT, pT);
        float p_w = min(exp(-(dist2)/p.phiPosition), 1.0);

        float weight = c_w * n_w * p_w;

        float kernelWeight = kernel[y * FILTER_SIZE + x];
        sum += ctmp * weight * kernelWeight;
        sumw += weight * kernelWeight;
      }
    }
  }

  imageStore(gFilteredImage, pixel, vec4(sum.xyz / sumw, 1.f));
}
