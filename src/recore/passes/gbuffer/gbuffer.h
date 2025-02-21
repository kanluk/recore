#pragma once

#include <recore/passes/pass.h>

#include <recore/scene/gpu_scene.h>

namespace recore::passes {

struct GBuffer {
  uPtr<vulkan::Image> position;
  uPtr<vulkan::Image> normal;
  uPtr<vulkan::Image> albedo;
  uPtr<vulkan::Image> emission;
  uPtr<vulkan::Image> motion;
  uPtr<vulkan::Image> material;
  uPtr<vulkan::Image> depth;
};

class GBufferPass : public Pass {
 public:
  explicit GBufferPass(const vulkan::Device& device,
                       const scene::GPUScene& scene);

  void resize(uint32_t width, uint32_t height) override;

  void reloadShaders(vulkan::ShaderLibrary& shaderLibrary) override;

  void execute(const vulkan::CommandBuffer& commandBuffer,
               vulkan::RenderFrame& currentFrame) override;

  [[nodiscard]] const GBuffer& getGBuffer() const { return mGBuffer; };

 private:
  const scene::GPUScene& mScene;

  uPtr<vulkan::RenderPass> mRenderPass;

  GBuffer mGBuffer;
  uPtr<vulkan::Framebuffer> mFramebuffer;

  uPtr<vulkan::PipelineLayout> mPipelineLayout;
  uPtr<vulkan::RasterPipeline> mPipeline;
};

}  // namespace recore::passes
