#ifndef HASHGRID_GLSL
#define HASHGRID_GLSL

#extension GL_EXT_control_flow_attributes : require

#include "hashgrid.glslh"

#include "bda.glsl"

#define hash hash_pcg32

// TEA hash
void hash_combine(inout uint val0, in uint val1) {
  uint v0 = val0;
  uint v1 = val1;
  uint s0 = 0;

  [[unroll]]
  for (uint n = 0; n < 16; n++) {
    s0 += 0x9e3779b9;
    v0 += ((v1 << 4) + 0xa341316c) ^ (v1 + s0) ^ ((v1 >> 5) + 0xc8013ea4);
    v1 += ((v0 << 4) + 0xad90777d) ^ (v0 + s0) ^ ((v0 >> 5) + 0x7e95761e);
  }

  val0 = v0;
}


ivec3 discretizePosition(float scale, vec3 position) {
    return ivec3(floor(vec3(position / scale)));
}

uint discretizeNormal(vec3 normal) {
  uint idx = 0;
  if (normal.x >= 0.0) idx |= 1u;
  if (normal.y >= 0.0) idx |= 2u;
  if (normal.z >= 0.0) idx |= 4u;
  return idx;
}

HashKF HashGrid_hash(HashGrid grid, vec3 position, vec3 normal) {
  ivec3 gridPosition = discretizePosition(grid.scale, position);
  uint gridNormal = discretizeNormal(normal);

  uint key = 0;
  hash_combine(key, gridPosition.x);
  hash_combine(key, gridPosition.y);
  hash_combine(key, gridPosition.z);

  uint fingerprint = key;
  hash_combine(fingerprint, gridNormal);

  return HashKF(key, fingerprint);
}

#define HASHGRID_FIND_TRIES 32
uint HashGrid_findOrInsert(HashGrid grid, HashKF hash) {
  uint key = hash.key % grid.size;
  uint fingerprint = hash.fingerprint;
  for (uint i = 0; i < HASHGRID_FIND_TRIES; i++) {
    uint currentFingerprint = atomicCompSwap(deref(grid.cells, key).kf.fingerprint, 0, fingerprint);
    if (currentFingerprint == 0 || currentFingerprint == fingerprint) {
      return key;
    }
    key++;
    key %= grid.size;
  }
  return -1;
}

uint HashGrid_find(HashGrid grid, HashKF hash) {
  uint key = hash.key % grid.size;
  uint fingerprint = hash.fingerprint;
  for (uint i = 0; i < HASHGRID_FIND_TRIES; i++) {
    uint currentFingerprint = deref(grid.cells, key).kf.fingerprint;
    if (currentFingerprint == fingerprint) {
      return key;
    }
    key++;
    key %= grid.size;
  }
  return -1;
}



#endif // HASHGRID_GLSL
