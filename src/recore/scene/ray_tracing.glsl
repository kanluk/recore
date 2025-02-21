#ifndef RAY_TRACING_GLSL
#define RAY_TRACING_GLSL

#include <recore/scene/scene.glsl>
#include <recore/shaders/constants.glsl>

struct Ray {
  vec3 origin;
  vec3 direction;
};

struct Hit {
  int instanceID;
  int primitiveID;
  vec2 barycentrics;
};


#ifdef ENABLE_RAY_TRACING
bool traceShadowRay(Ray ray) {
  rayQueryEXT rayQuery;
  rayQueryInitializeEXT(rayQuery, gTLAS, gl_RayFlagsTerminateOnFirstHitEXT | gl_RayFlagsOpaqueEXT, 0xFF, ray.origin, M_EPSILON, ray.direction, 1.0 - M_EPSILON);

  rayQueryProceedEXT(rayQuery);
  return rayQueryGetIntersectionTypeEXT(rayQuery, true) != gl_RayQueryCommittedIntersectionNoneEXT;
}

bool traceRay(Ray ray, out Hit hit) {
  rayQueryEXT rayQuery;
  rayQueryInitializeEXT(rayQuery, gTLAS, gl_RayFlagsOpaqueEXT, 0xFF, ray.origin, M_EPSILON, ray.direction, 10000.0);

  while (rayQueryProceedEXT(rayQuery)) {
    rayQueryConfirmIntersectionEXT(rayQuery);
  }

  if (rayQueryGetIntersectionTypeEXT(rayQuery, true) == gl_RayQueryCommittedIntersectionNoneEXT) {
    return false; // Miss
  }

  hit.instanceID = rayQueryGetIntersectionInstanceIdEXT(rayQuery, true);
  hit.primitiveID = rayQueryGetIntersectionPrimitiveIndexEXT(rayQuery, true);
  hit.barycentrics = rayQueryGetIntersectionBarycentricsEXT(rayQuery, true);

  return true;
}
#else

bool traceShadowRay(Ray ray) { return false; }
bool traceRay(Ray ray, out Hit hit) { return false; }

#endif


#endif // RAY_TRACING_GLSL
