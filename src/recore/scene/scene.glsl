#ifndef SCENE_GLSL
#define SCENE_GLSL

#extension GL_EXT_nonuniform_qualifier : require

#ifdef ENABLE_RAY_TRACING
#extension GL_EXT_ray_query : enable
#endif

#include "scene.glslh"

// Bindings / Resources

layout(set = 0, binding = 0) uniform sampler2D gTextures[];

#ifdef ENABLE_RAY_TRACING
layout(set = 0, binding = 1) uniform accelerationStructureEXT gTLAS;
#endif


#endif // SCENE_GLSL
