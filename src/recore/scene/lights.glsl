#ifndef LIGHTS_GLSL
#define LIGHTS_GLSL

struct PointLight {
  vec3 position;
  vec3 color;
  float intensity;
};

vec3 PointLight_eval(PointLight light, vec3 position) {
  vec3 L = light.position - position;
  float distance = length(L);
  L = normalize(L);
  float attenuation = 1.0 / (distance * distance);
  return light.color * light.intensity * attenuation;
}

struct DirectionalLight {
  vec3 direction;
  vec3 color;
  float intensity;
};

vec3 DirectionalLight_eval(DirectionalLight light) {
  return light.color * light.intensity;
}



#endif // LIGHTS_GLSL
