#ifndef DISTRIBUTIONVIS_GLSL
#define DISTRIBUTIONVIS_GLSL

#include <recore/shaders/bda.glsl>
#include <recore/shaders/vmm.glsl>
#include <recore/passes/lpv/lpv.glsl>

layout(buffer_reference, scalar, buffer_reference_align = 4) buffer DistributionBuffer { vec3 v[]; };

#define DISTRIBUTIONVIS_NUM_BINS 600
void DistributionVis_plotVMM(DistributionBuffer vis, VMM vmm, vec3 position) {
  vec3 lastPosition = deref(vis, DISTRIBUTIONVIS_NUM_BINS);
  deref(vis, DISTRIBUTIONVIS_NUM_BINS) = position;

  for (int i = 0; i < DISTRIBUTIONVIS_NUM_BINS; i++) {
    vec3 direction = normalize(deref(vis, i) - lastPosition);
    float pdf = max(0.05, VMM_pdf(vmm, direction));
    deref(vis, i) = direction * pdf + position;
  }
}

#endif // DISTRIBUTIONVIS_GLSL
