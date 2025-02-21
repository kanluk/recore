#include "rsm.h"

#include <recore/vulkan/debug.h>

#include "rsm.glslh"

namespace recore::passes {

constexpr auto kRSMVertexShader = "recore/passes/rsm/rsm.vert.glsl";

constexpr auto kRSMFragmentShader = "recore/passes/rsm/rsm.frag.glsl";

RSMPass::RSMPass(const vulkan::Device& device,
                 const scene::GPUScene& scene,
                 uint32_t width,
                 uint32_t height)
    : Pass{device}, mScene{scene}, mWidth{width}, mHeight{height} {
  mRenderPass = makeUnique<vulkan::RenderPass>(vulkan::RenderPass::Desc{
      .device = mDevice,
      .attachments = {
          // POSITION
          {VK_FORMAT_R32G32B32A32_SFLOAT,
           VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL},
          // NORMAL
          {VK_FORMAT_R32G32B32A32_SFLOAT,
           VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL},
          // FLUX
          {VK_FORMAT_R32G32B32A32_SFLOAT,
           VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL},
          // DEPTH
          {VK_FORMAT_D32_SFLOAT,
           VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL},
      }});

  auto genImage = [&](VkFormat format, VkImageUsageFlags usage = 0) {
    return makeUnique<vulkan::Image>({
        .device = mDevice,
        .format = format,
        .extent = {width, height, 1},
        .usage = VK_IMAGE_USAGE_SAMPLED_BIT | usage,
        .memoryUsage = VMA_MEMORY_USAGE_GPU_ONLY,
    });
  };

  constexpr VkFormat RSM_FORMAT = VK_FORMAT_R32G32B32A32_SFLOAT;
  constexpr VkImageUsageFlags RSM_USAGE = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT;

  mRSM.position = genImage(RSM_FORMAT, RSM_USAGE);
  mRSM.normal = genImage(RSM_FORMAT, RSM_USAGE);
  mRSM.flux = genImage(RSM_FORMAT, RSM_USAGE);
  mRSM.depth = genImage(VK_FORMAT_D32_SFLOAT,
                        VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT);

  mFramebuffer = makeUnique<vulkan::Framebuffer>(
      {.device = mDevice,
       .renderPass = *mRenderPass,
       .attachments = {&mRSM.position->getView(),
                       &mRSM.normal->getView(),
                       &mRSM.flux->getView(),
                       &mRSM.depth->getView()},
       .width = width,
       .height = height});
}

void RSMPass::reloadShaders(vulkan::ShaderLibrary& shaderLibrary) {
  mPipelineLayout = makeUnique<vulkan::PipelineLayout>(
      {.device = mDevice,
       .descriptorSetLayouts = {&mScene.getDescriptorSetLayout()},
       .pushConstants = {
           {.stageFlags = VK_SHADER_STAGE_ALL, .size = sizeof(RSMPush)},
       }});

  mPipeline = makeUnique<vulkan::RasterPipeline>({
      .device = mDevice,
      .layout = *mPipelineLayout,
      .renderPass = *mRenderPass,
      .state =
          {
              .vertexBindingDescriptions = {VkVertexInputBindingDescription{
                  0, sizeof(Vertex), VK_VERTEX_INPUT_RATE_VERTEX}},
              .vertexAttributeDescriptions = {VkVertexInputAttributeDescription{
                                                  0,
                                                  0,
                                                  VK_FORMAT_R32G32B32_SFLOAT,
                                                  offsetof(Vertex, position)},
                                              VkVertexInputAttributeDescription{
                                                  1,
                                                  0,
                                                  VK_FORMAT_R32G32B32_SFLOAT,
                                                  offsetof(Vertex, normal)},
                                              VkVertexInputAttributeDescription{
                                                  2,
                                                  0,
                                                  VK_FORMAT_R32G32_SFLOAT,
                                                  offsetof(Vertex, texCoord)}},
              .cullMode = VK_CULL_MODE_NONE,
              .depthTest = true,
              .depthWrite = true,
              .colorAttachmentCount = 3,
          },
      .shaders = {shaderLibrary.loadShader(kRSMVertexShader),
                  shaderLibrary.loadShader(kRSMFragmentShader)},
  });
}

void RSMPass::execute(const vulkan::CommandBuffer& commandBuffer,
                      vulkan::RenderFrame& currentFrame) {
  RECORE_GPU_PROFILE_SCOPE(currentFrame, commandBuffer, "RSMPass::draw");

  commandBuffer.setFramebufferSize(mWidth, mHeight);

  commandBuffer.beginRenderPass(*mRenderPass,
                                *mFramebuffer,
                                {
                                    {.color = {0.f, 0.f, 0.f, 0.f}},
                                    {.color = {0.f, 0.f, 0.f, 0.f}},
                                    {.color = {0.f, 0.f, 0.f, 0.f}},
                                    {.depthStencil = {1.f, 0}},
                                });

  commandBuffer.bindPipeline(*mPipeline);
  commandBuffer.bindVertexBuffer(mScene.getVertexBuffer());
  commandBuffer.bindIndexBuffer(mScene.getIndexBuffer());

  commandBuffer.bindDescriptorSet(*mPipeline, mScene.getDescriptorSet(), 0);

  const auto& cpuScene = mScene.getScene();

  const auto& geometryInstances = cpuScene.getGeometryInstances();
  const auto& meshes = cpuScene.getMeshes();
  const auto& modelMatrices = cpuScene.getModelMatrices();

  struct RSMPush p {
    .scene = mScene.getSceneDataDeviceAddress(), .geometryInstanceID = 0,
  };

  for (size_t i = 0; i < geometryInstances.size(); ++i) {
    const auto& instance = geometryInstances.at(i);
    const auto& mesh = meshes.at(instance.meshID);

    p.geometryInstanceID = i;

    commandBuffer.pushConstants(*mPipelineLayout, p);
    commandBuffer.drawIndexed(mesh.indexCount, 1, mesh.firstIndex, 0, 0);
  }

  commandBuffer.endRenderPass();
}

}  // namespace recore::passes
