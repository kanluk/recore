#version 460

layout(local_size_x = 32, local_size_y = 32, local_size_z = 1) in;

layout(set = 0, binding = 0, rgba32f) uniform writeonly image2D gOutputImage;

layout(set = 0, binding = 1) uniform sampler2D gMotion;
layout(set = 0, binding = 2) uniform sampler2D gPrevFrame;
layout(set = 0, binding = 3) uniform sampler2D gCurrentFrame;

#define SAMPLE_MOTION_RADIUS 1
#define ESTIMATE_BOUNDS_RADIUS 1
#define ALPHA 0.1f
// #define USE_MOMENTS


vec2 sampleMotionVector(ivec2 pixelCoord, int radius) {
  vec2 longestMotion = vec2(0.0, 0.0);
  float longestLength2 = -1.0;

  for (int y = -radius; y <= radius; y++) {
    for (int x = -radius; x <= radius; x++) {
      vec2 motion = texelFetch(gMotion, pixelCoord + ivec2(x, y), 0).rg;
      float motionLength2 = dot(motion, motion);
      if (motionLength2 > longestLength2) {
        longestMotion = motion;
        longestLength2 = motionLength2;
      }
    }
  }

  return longestMotion;
}

void estimateBoundsMinMax(ivec2 pixelCoord, int radius, out vec3 minColor, out vec3 maxColor) {
  minColor = vec3(99999.0);
  maxColor = vec3(-99999.0);

  for (int y = -radius; y <= radius; y++) {
    for (int x = -radius; x <= radius; x++) {
      vec3 color = texelFetch(gCurrentFrame, pixelCoord + ivec2(x, y), 0).rgb;
      minColor = min(minColor, color);
      maxColor = max(maxColor, color);
    }
  }
}

void estimateBoundsMoments(ivec2 pixelCoord, int radius, out vec3 minColor, out vec3 maxColor) {
  vec3 moment1 = vec3(0.0);
  vec3 moment2 = vec3(0.0);

  for (int y = -radius; y <= radius; y++) {
    for (int x = -radius; x <= radius; x++) {
      vec3 color = texelFetch(gCurrentFrame, pixelCoord + ivec2(x, y), 0).rgb;
      moment1 += color;
      moment2 += color * color;
    }
  }

  moment1 /= float((2 * radius + 1) * (2 * radius + 1));
  moment2 /= float((2 * radius + 1) * (2 * radius + 1));

  vec3 sigma = sqrt(max(vec3(0.0), moment2 - moment1 * moment1));

  minColor = moment1 - sigma;
  maxColor = moment1 + sigma;
}

vec3 mergeColors(ivec2 pixel, vec3 currentColor, vec3 previousColor, float alpha) {

  vec3 minColor, maxColor;

#ifdef USE_MOMENTS
    estimateBoundsMoments(pixel, ESTIMATE_BOUNDS_RADIUS, minColor, maxColor);
#else
    estimateBoundsMinMax(pixel, ESTIMATE_BOUNDS_RADIUS, minColor, maxColor);
#endif

  previousColor = clamp(previousColor, minColor, maxColor);

  return mix(previousColor, currentColor, alpha);
}

// note: entirely stolen from https://gist.github.com/TheRealMJP/c83b8c0f46b63f3a88a5986f4fa982b1
//
// Samples a texture with Catmull-Rom filtering, using 9 texture fetches instead of 16.
// See http://vec3.ca/bicubic-filtering-in-fewer-taps/ for more details
vec4 sampleCatmullRom(sampler2D tex, vec2 uv)
{
  // We're going to sample a a 4x4 grid of texels surrounding the target UV coordinate. We'll do this by rounding
  // down the sample location to get the exact center of our "starting" texel. The starting texel will be at
  // location [1, 1] in the grid, where [0, 0] is the top left corner.
  vec2 texSize = textureSize(tex, 0);
  vec2 samplePos = uv * texSize;
  vec2 texPos1 = floor(samplePos - 0.5) + 0.5;

  // Compute the fractional offset from our starting texel to our original sample location, which we'll
  // feed into the Catmull-Rom spline function to get our filter weights.
  vec2 f = samplePos - texPos1;

  // Compute the Catmull-Rom weights using the fractional offset that we calculated earlier.
  // These equations are pre-expanded based on our knowledge of where the texels will be located,
  // which lets us avoid having to evaluate a piece-wise function.
  vec2 w0 = f * (-0.5 + f * (1.0 - 0.5*f));
  vec2 w1 = 1.0 + f * f * (-2.5 + 1.5*f);
  vec2 w2 = f * (0.5 + f * (2.0 - 1.5*f));
  vec2 w3 = f * f * (-0.5 + 0.5 * f);

  // Work out weighting factors and sampling offsets that will let us use bilinear filtering to
  // simultaneously evaluate the middle 2 samples from the 4x4 grid.
  vec2 w12 = w1 + w2;
  vec2 offset12 = w2 / (w1 + w2);

  // Compute the final UV coordinates we'll use for sampling the texture
  vec2 texPos0 = texPos1 - vec2(1.0);
  vec2 texPos3 = texPos1 + vec2(2.0);
  vec2 texPos12 = texPos1 + offset12;

  texPos0 /= texSize;
  texPos3 /= texSize;
  texPos12 /= texSize;

  vec4 result = vec4(0.0);
  result += textureLod(tex, vec2(texPos0.x, texPos0.y), 0) * w0.x * w0.y;
  result += textureLod(tex, vec2(texPos12.x, texPos0.y), 0) * w12.x * w0.y;
  result += textureLod(tex, vec2(texPos3.x, texPos0.y), 0) * w3.x * w0.y;

  result += textureLod(tex, vec2(texPos0.x, texPos12.y), 0) * w0.x * w12.y;
  result += textureLod(tex, vec2(texPos12.x, texPos12.y), 0) * w12.x * w12.y;
  result += textureLod(tex, vec2(texPos3.x, texPos12.y), 0) * w3.x * w12.y;

  result += textureLod(tex, vec2(texPos0.x, texPos3.y), 0) * w0.x * w3.y;
  result += textureLod(tex, vec2(texPos12.x, texPos3.y), 0) * w12.x * w3.y;
  result += textureLod(tex, vec2(texPos3.x, texPos3.y), 0) * w3.x * w3.y;

  return result;
}

void main() {
  ivec2 pixel = ivec2(gl_GlobalInvocationID.xy);
  ivec2 resolution = imageSize(gOutputImage);

  if (any(greaterThanEqual(pixel, resolution))) {
    return;
  }

  vec2 motion = sampleMotionVector(pixel, SAMPLE_MOTION_RADIUS);

  vec3 currentColor = texelFetch(gCurrentFrame, pixel, 0).rgb;
  vec2 texCoord = (vec2(pixel) + 0.5f) / vec2(resolution);
  vec3 previousColor = sampleCatmullRom(gPrevFrame, texCoord + motion).rgb;

  vec3 mergedColor = mergeColors(pixel, currentColor, previousColor, ALPHA);

  imageStore(gOutputImage, pixel, vec4(mergedColor, 1.f));
  // imageStore(gOutputImage, pixel, vec4(currentColor, 1.f));
}
