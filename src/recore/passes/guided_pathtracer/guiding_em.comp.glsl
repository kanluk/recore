#version 460


#include "guiding.glslh"


#include <recore/shaders/hashgrid.glsl>
#include <recore/shaders/vmm.glsl>

layout(local_size_x = 64, local_size_y = 1, local_size_z = 1) in;

PUSH_CONSTANT(GuidingEMPush);

void main() {
  uint key = gl_GlobalInvocationID.x;

  if (key == 0 && key >= p.hashGrid.size) {
    return;
  }

  uint cellCounter = deref(p.cellCounters, key);
  if (cellCounter == 0) {
    return;
  }


  uint baseIndex = deref(p.cellPrefixSums, key);
  baseIndex -= cellCounter;
  uint maxSamples = min(cellCounter, 3200);

  VMM vmm = deref(p.vmms, key);


  // Temporal decay
  if (maxSamples > 0) {

    const float alpha = 0.95f;
    vmm.numSamples *= alpha;

    for (int i = 0; i < VMM_NUM_COMPONENTS; i++) {
      vmm.components[i].sumWeightedDirections *= alpha;
      vmm.components[i].sumWeights *= alpha;
    }
  }

  for (uint i = 0; i < maxSamples; i++) {
    GuidingSample guidingSample = deref(p.guidingSamples, deref(p.cellIndices, baseIndex + i));
    VMM_Sample vmmSample = VMM_Sample(guidingSample.direction, guidingSample.weight);
    VMM_EM(vmm, vmmSample);
  }

  deref(p.vmms, key) = vmm;
}
