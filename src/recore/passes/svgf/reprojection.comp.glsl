#version 460

// Input
layout(set = 0, binding = 0) uniform sampler2D gGPosition;
layout(set = 0, binding = 1) uniform sampler2D gGNormal;
layout(set = 0, binding = 2) uniform sampler2D gGAlbedo;
layout(set = 0, binding = 3) uniform sampler2D gGMotion;
layout(set = 0, binding = 4) uniform sampler2D gIllumination;

layout(set = 0, binding = 10) uniform sampler2D gPrevGPosition;
layout(set = 0, binding = 11) uniform sampler2D gPrevGNormal;
layout(set = 0, binding = 12) uniform sampler2D gPrevGAlbedo;
layout(set = 0, binding = 13) uniform sampler2D gPrevIllumination;
layout(set = 0, binding = 14) uniform sampler2D gPrevHistoryLength;

// Output
layout(set = 0, binding = 20, rgba32f) uniform writeonly image2D gOutputIllumination;
layout(set = 0, binding = 21, r32f) uniform writeonly image2D gOutputHistoryLength;


#define MAX_POSITION_DIFFERENCE 0.25
#define MAX_NORMAL_DIFFERENCE 0.25

bool isPositionDifferenceValid(vec3 position, vec3 prevPosition) {
  vec3 difference = prevPosition - position;
  return dot(difference, difference) < (MAX_POSITION_DIFFERENCE * MAX_POSITION_DIFFERENCE);
}

bool isNormalDifferenceValid(vec3 normal, vec3 prevNormal) {
  return dot(normal, prevNormal) > (1.f - MAX_NORMAL_DIFFERENCE);
}

bool isReprojectionValid(vec2 motion, vec4 position, vec4 prevPosition, vec3 normal, vec3 prevNormal, vec3 albedo, vec3 prevAlbedo) {
  if (position.w < 1.f) return false;
  if (prevPosition.w < 1.f) return false;

  float motionDist = length(motion);
  if (motionDist < 1e-9) return true;

  if (!isPositionDifferenceValid(position.xyz, prevPosition.xyz)) return false;
  if (!isNormalDifferenceValid(normal, prevNormal)) return false;
  // if (length(albedo.xyz - prevAlbedo.xyz) > 0.05) return false;

  return true;
}


vec3 demodulate(vec3 color, vec3 albedo) {
  return color / max(albedo, vec3(0.0001));
}


void main() {
  ivec2 pixel = ivec2(gl_GlobalInvocationID.xy);
  ivec2 resolution = textureSize(gGPosition, 0);

  if (any(greaterThanEqual(pixel, resolution))) {
    return;
  }


  // Load current pixel data
  vec3 illumination = texelFetch(gIllumination, pixel, 0).rgb;
  vec4 position = texelFetch(gGPosition, pixel, 0);
  vec3 normal = normalize(texelFetch(gGNormal, pixel, 0).rgb);
  vec4 albedo = texelFetch(gGAlbedo, pixel, 0);
  vec2 motion = texelFetch(gGMotion, pixel, 0).xy;


  // Use motion vector to find the corresponding pixel in the previous frame
  vec2 prevPixelPos = vec2(pixel) + motion * vec2(resolution);
  ivec2 prevPixel = ivec2(floor(prevPixelPos));

  // Sample previous pixel data
  bool valid = false;
  bool validSample[4];
  const ivec2 offset[4] = { ivec2(0, 0), ivec2(1, 0), ivec2(0, 1), ivec2(1, 1) };
  for (int i = 0; i < 4; i++) {
    ivec2 samplePixel = prevPixel + offset[i];



    vec4 prevPosition = texelFetch(gPrevGPosition, samplePixel, 0);
    vec3 prevNormal = normalize(texelFetch(gPrevGNormal, samplePixel, 0).rgb);
    vec4 prevAlbedo = texelFetch(gPrevGAlbedo, samplePixel, 0);

    validSample[i] = isReprojectionValid(motion, position, prevPosition, normal, prevNormal, albedo.rgb, prevAlbedo.rgb);

    valid = valid || validSample[i];
  }

  vec3 prevIllumination = vec3(0.0);
  float prevHistory = texelFetch(gPrevHistoryLength, prevPixel, 0).r;

  // If at least one sample was valid, store the reprojection result
  if (valid) {
    float sumw = 0.f;
    float x = fract(prevPixelPos.x);
    float y = fract(prevPixelPos.y);

    // Use bilinear filter
    const float w[4] = { (1 - x) * (1 - y), x  * (1 - y), (1 - x) * y, x * y };

    for (int i = 0; i < 4; i++) {
      ivec2 samplePixel = prevPixel + offset[i];
      if (validSample[i]) {
        vec3 prevIlluminationSample = texelFetch(gPrevIllumination, samplePixel, 0).rgb;
        prevIllumination += prevIlluminationSample * w[i];
        sumw += w[i];
      }
    }

    valid = (sumw > 0.01f);
    if (valid) {
      prevIllumination /= sumw;
    }

  } else {
    // TODO: Extend filter range to find a valid sample
  }

  if (!valid) {
    prevIllumination = vec3(0.0);
    prevHistory = 0.f;
  }

  // Perform albedo demodulation
  if (albedo.w >= 1.0) {
    illumination = demodulate(illumination, albedo.rgb);
  } else {
    valid = false;
  }

  // Store the result
  float outHistoryLength = prevHistory + 1.f;
  float weight = 1.f / outHistoryLength;
  float alpha = valid ? max(0.05f, weight) : 1.f;
  // alpha = 1.f;

  imageStore(gOutputIllumination, pixel, vec4(mix(prevIllumination, illumination, alpha), 1.0));
  imageStore(gOutputHistoryLength, pixel, vec4(outHistoryLength, 0.f, 0.f, 0.f));
}
