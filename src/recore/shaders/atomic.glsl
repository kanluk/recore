#ifndef ATOMIC_GLSL
#define ATOMIC_GLSL


#define atomicAddVec3(address, value) \
  atomicAdd(address.x, value.x); \
  atomicAdd(address.y, value.y); \
  atomicAdd(address.z, value.z);

#endif // ATOMIC_GLSL
