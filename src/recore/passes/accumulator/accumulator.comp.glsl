#version 450

#include "accumulator.glslh"

#include <recore/shaders/math.glsl>

layout(local_size_x = 32, local_size_y = 32, local_size_z = 1) in;

layout(set = 0, binding = 0, rgba32f) uniform readonly image2D gInputImage;
layout(set = 0, binding = 1, rgba32f) uniform image2D gOutputImage;
layout(set = 0, binding = 2, rgba32f) uniform image2D gVarianceImage;

PUSH_CONSTANT(AccumulatorPush);

void main() {
  if (p.maxSamples > 0 && p.frameCount > p.maxSamples) {
    return;
  }

  ivec2 pixel = ivec2(gl_GlobalInvocationID.xy);
  ivec2 resolution = imageSize(gOutputImage);

  if (any(greaterThanEqual(pixel, resolution))) {
    return;
  }


  // Accumulation weight
  float weight = 1.f / float(p.frameCount + 1);

  // Load color, accumulator and moments
  vec3 color = imageLoad(gInputImage, pixel).xyz;
  vec3 accum = imageLoad(gOutputImage, pixel).xyz;
  vec2 moments = imageLoad(gVarianceImage, pixel).xy;

  // Check for NaN or inf
  if (ISINF_OR_ISNAN(color)) {
    color = vec3(0.f);
  }
  if (ISINF_OR_ISNAN(accum)) {
    accum = vec3(1000.f, 0.f, 0.f);
  }

  // Update color accumulator
  accum = mix(accum, color, weight);

  // Update moments
  float averageColor = average(color);
  float averageColorSquared = averageColor * averageColor;
  moments.x = mix(moments.x, averageColor, weight);
  moments.y = mix(moments.y, averageColorSquared, weight);

  // Store accumulator and moments
  imageStore(gOutputImage, pixel, vec4(accum, 1.f));
  imageStore(gVarianceImage, pixel, vec4(moments, 0.f, 0.f));
}
