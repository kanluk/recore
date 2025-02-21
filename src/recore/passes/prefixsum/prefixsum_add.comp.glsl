#version 460

#extension GL_KHR_shader_subgroup_basic : require
#extension GL_KHR_shader_subgroup_arithmetic : require

#include "prefixsum.glslh"

layout(local_size_x = PREFIX_SUM_LOCAL_SIZE, local_size_y = 1, local_size_z = 1) in;

PUSH_CONSTANT(PrefixSumPush);

void main() {
  const uint localIdx = gl_LocalInvocationID.x;
  const uint workgroupIdx = gl_WorkGroupID.x;
  const uint workgroupSize = gl_WorkGroupSize.x;

  uint prefixSum = deref(p.workgroupPrefixSums, workgroupIdx >> 1);
  const uint elementIdx = (workgroupIdx * workgroupSize) + (2 * workgroupSize * (p.iteration * workgroupSize + 1)) + localIdx;

  if (elementIdx < p.size) {
    uint v = deref(p.data, elementIdx);
    v += prefixSum;
    deref(p.data, elementIdx) = v;
  }
}
