#version 460

layout(set = 0, binding = 0) uniform sampler2D gAlbedo;
layout(set = 0, binding = 1) uniform sampler2D gIllumination;

layout(set = 0, binding = 2, rgba32f) uniform writeonly image2D gOutputImage;

void main() {
  ivec2 pixel = ivec2(gl_GlobalInvocationID.xy);
  ivec2 resolution = textureSize(gAlbedo, 0);

  if (any(greaterThanEqual(pixel, resolution))) {
    return;
  }

  vec3 albedo = texelFetch(gAlbedo, pixel, 0).rgb;
  vec3 illumination = texelFetch(gIllumination, pixel, 0).rgb;

  vec3 color = albedo * illumination;

  imageStore(gOutputImage, pixel, vec4(color, 1.0));
}
