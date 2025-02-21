#version 460

#extension GL_EXT_debug_printf : require

#include "guiding.glslh"

layout(local_size_x = 32, local_size_y = 1, local_size_z = 1) in;

PUSH_CONSTANT(GuidingIndicesPush);

void main() {
  uint index = gl_GlobalInvocationID.x;

  if (index >= p.numGuidingSamples) {
    return;
  }

  GuidingSample guidingSample = deref(p.guidingSamples, index);

  // Check if sample is valid
  if (guidingSample.key == 0) {
    return;
  }

  uint baseOffset = deref(p.cellPrefixSums, guidingSample.key);
  uint cellCounter = deref(p.cellCounters, guidingSample.key);
  baseOffset -= cellCounter;

  deref(p.cellIndices, baseOffset + guidingSample.indexInCell) = index;
}
