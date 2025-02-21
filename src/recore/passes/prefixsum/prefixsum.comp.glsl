#version 460

#include "prefixsum.glslh"


#extension GL_KHR_shader_subgroup_basic : require
#extension GL_KHR_shader_subgroup_arithmetic : require


layout(local_size_x = PREFIX_SUM_LOCAL_SIZE, local_size_y = 1, local_size_z = 1) in;

PUSH_CONSTANT(PrefixSumPush);

shared uint sharedPrefixSum[2 * PREFIX_SUM_LOCAL_SIZE];

void main() {
  const uint localIdx = gl_LocalInvocationID.x;
  const uint subgroupLocalIdx = gl_SubgroupInvocationID;
  const uint workgroupIdx = gl_WorkGroupID.x;
  const uint subgroupIdx = gl_SubgroupID;
  const uint workgroupSize = gl_WorkGroupSize.x;
  const uint subgroupSize = gl_SubgroupSize;
  const uint numWorkgroups = gl_NumWorkGroups.x;
  const uint numSubgroups = gl_NumSubgroups;

  const uint prevSum = deref(p.prevTotalSum, 0);
  const uint elementIdx0 = 2 * workgroupSize * workgroupSize * p.iteration + (2 * workgroupSize * workgroupIdx) + localIdx;
  const uint elementIdx1 = elementIdx0 + workgroupSize;


  sharedPrefixSum[subgroupIdx] = 0;
  sharedPrefixSum[subgroupIdx + numSubgroups] = 0;

  // Each thread loads two elements from data
  uint v0 = (elementIdx0 < p.size) ? deref(p.data, elementIdx0) : 0;
  uint v1 = (elementIdx1 < p.size) ? deref(p.data, elementIdx1) : 0;

  // Perform prefix sum in subgroup
  uint sum0 = subgroupInclusiveAdd(v0);
  uint sum1 = subgroupInclusiveAdd(v1);

  // Last thread in subgroup writes the sum to shared memory
  if (subgroupLocalIdx == gl_SubgroupSize - 1) {
    sharedPrefixSum[subgroupIdx] = sum0;
    sharedPrefixSum[subgroupIdx + numSubgroups] = sum1;
  }

  barrier();

  // Perform prefix sum over shared memory (first two subgroups)
  if (subgroupIdx == 0) {
    uint v = (subgroupLocalIdx < 2 * numSubgroups) ? sharedPrefixSum[subgroupLocalIdx] : 0;
    uint sum = subgroupInclusiveAdd(v);
    sharedPrefixSum[subgroupLocalIdx] = sum;
  }
  #if PREFIX_SUM_LOCAL_SIZE == 1024
  else if (subgroupIdx == 1) {
    uint sharedIndex = subgroupLocalIdx + numSubgroups;
    uint v = (sharedIndex < 2 * numSubgroups) ? sharedPrefixSum[sharedIndex] : 0;
    uint sum = subgroupInclusiveAdd(v);
    sharedPrefixSum[sharedIndex] = sum;
  }
  #endif

  barrier();

  // Add shared prefixes to local prefixes
  uint blockSum0 = (subgroupIdx > 0) ? sharedPrefixSum[subgroupIdx - 1] : 0;
  #if PREFIX_SUM_LOCAL_SIZE == 1024
  uint blockSum1 = (subgroupIdx > 0) ? sharedPrefixSum[subgroupIdx + numSubgroups - 1] : 0;
  #else
  uint blockSum1 = sharedPrefixSum[subgroupIdx + numSubgroups - 1];
  #endif

  sum0 += blockSum0;
  #if PREFIX_SUM_LOCAL_SIZE == 1024
  sum1 += blockSum1 + sharedPrefixSum[numSubgroups - 1];
  #else
  sum1 += blockSum1;
  #endif

  #if PREFIX_SUM_LOCAL_SIZE == 1024
  barrier();

  if (localIdx == workgroupSize - 1) {
    sharedPrefixSum[0] = sum1;
  }
  #endif

  barrier();

  #if PREFIX_SUM_LOCAL_SIZE == 1024
  uint groupPrefixSum = sharedPrefixSum[0];
  #else
  uint groupPrefixSum = sharedPrefixSum[2 * numSubgroups - 1];
  #endif

  if (localIdx >= workgroupIdx && localIdx < numWorkgroups) {
    atomicAdd(deref(p.workgroupPrefixSums, localIdx), groupPrefixSum);
    if (localIdx == numWorkgroups - 1) {
      atomicAdd(deref(p.totalSum, 0), groupPrefixSum);
    }
  }

  // Store result
  if (elementIdx0 < p.size) {
    deref(p.data, elementIdx0) = sum0 + prevSum;
  }
  if (elementIdx1 < p.size) {
    deref(p.data, elementIdx1) = sum1 + prevSum;
  }
}
