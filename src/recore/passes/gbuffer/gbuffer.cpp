#include "gbuffer.h"

#include <recore/vulkan/debug.h>

namespace recore::passes {

constexpr auto kGBufferVertexShader = "recore/passes/gbuffer/gbuffer.vert.glsl";

constexpr auto kGBufferFragmentShader =
    "recore/passes/gbuffer/gbuffer.frag.glsl";

GBufferPass::GBufferPass(const vulkan::Device& device,
                         const scene::GPUScene& scene)
    : Pass{device}, mScene{scene} {
  mRenderPass = makeUnique<vulkan::RenderPass>(
      {.device = mDevice,
       .attachments = {
           {VK_FORMAT_R32G32B32A32_SFLOAT,
            VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL},
           {VK_FORMAT_R32G32B32A32_SFLOAT,
            VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL},
           {VK_FORMAT_R32G32B32A32_SFLOAT,
            VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL},
           {VK_FORMAT_R32G32B32A32_SFLOAT,
            VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL},
           {VK_FORMAT_R32G32_SFLOAT, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL},
           {VK_FORMAT_R32G32B32A32_SFLOAT,
            VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL},
           {VK_FORMAT_D32_SFLOAT,
            VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL},
       }});
}

void GBufferPass::resize(uint32_t width, uint32_t height) {
  auto genImage = [&](VkFormat format, VkImageUsageFlags usage = 0) {
    return makeUnique<vulkan::Image>({
        .device = mDevice,
        .format = format,
        .extent = {width, height, 1},
        .usage = VK_IMAGE_USAGE_SAMPLED_BIT | VK_IMAGE_USAGE_TRANSFER_SRC_BIT |
                 usage,
        .memoryUsage = VMA_MEMORY_USAGE_GPU_ONLY,
    });
  };

  constexpr VkFormat GBUFFER_FORMAT = VK_FORMAT_R32G32B32A32_SFLOAT;
  constexpr VkImageUsageFlags GBUFFER_USAGE =
      VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT;

  mGBuffer.position = genImage(GBUFFER_FORMAT, GBUFFER_USAGE);
  mGBuffer.normal = genImage(GBUFFER_FORMAT, GBUFFER_USAGE);
  mGBuffer.albedo = genImage(GBUFFER_FORMAT, GBUFFER_USAGE);
  mGBuffer.emission = genImage(GBUFFER_FORMAT, GBUFFER_USAGE);
  mGBuffer.motion = genImage(VK_FORMAT_R32G32_SFLOAT, GBUFFER_USAGE);
  mGBuffer.material = genImage(GBUFFER_FORMAT, GBUFFER_USAGE);
  mGBuffer.depth = genImage(VK_FORMAT_D32_SFLOAT,
                            VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT);

  mFramebuffer = makeUnique<vulkan::Framebuffer>(
      {.device = mDevice,
       .renderPass = *mRenderPass,
       .attachments = {&mGBuffer.position->getView(),
                       &mGBuffer.normal->getView(),
                       &mGBuffer.albedo->getView(),
                       &mGBuffer.emission->getView(),
                       &mGBuffer.motion->getView(),
                       &mGBuffer.material->getView(),
                       &mGBuffer.depth->getView()},
       .width = width,
       .height = height});
}

void GBufferPass::reloadShaders(vulkan::ShaderLibrary& shaderLibrary) {
  mPipelineLayout = makeUnique<vulkan::PipelineLayout>(
      {.device = mDevice,
       .descriptorSetLayouts = {&mScene.getDescriptorSetLayout()},
       .pushConstants = {
           // just overallocate for now
           {.stageFlags = VK_SHADER_STAGE_ALL,
            .size = vulkan::MAX_PUSH_CONSTANT_SIZE},
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
              .cullMode = VK_CULL_MODE_BACK_BIT,
              .depthTest = true,
              .depthWrite = true,
              .colorAttachmentCount = 6,
          },
      .shaders = {shaderLibrary.loadShader(kGBufferVertexShader),
                  shaderLibrary.loadShader(kGBufferFragmentShader)},
  });
}

void GBufferPass::execute(const vulkan::CommandBuffer& commandBuffer,
                          vulkan::RenderFrame& currentFrame) {
  RECORE_GPU_PROFILE_SCOPE(currentFrame, commandBuffer, "GBufferPass::draw");

  commandBuffer.beginRenderPass(*mRenderPass,
                                *mFramebuffer,
                                {
                                    {.color = {0.f, 0.f, 0.f, 0.f}},
                                    {.color = {0.f, 0.f, 0.f, 0.f}},
                                    {.color = {0.f, 0.f, 0.f, 0.f}},
                                    {.color = {0.f, 0.f, 0.f, 0.f}},
                                    {.color = {0.f, 0.f, 0.f, 0.f}},
                                    {.color = {0.f, 0.f, 0.f, 0.f}},
                                    {.depthStencil = {1.f, 0}},
                                });

  commandBuffer.bindPipeline(*mPipeline);
  commandBuffer.bindVertexBuffer(mScene.getVertexBuffer());
  commandBuffer.bindIndexBuffer(mScene.getIndexBuffer());
  commandBuffer.bindDescriptorSet(
      *mPipelineLayout, mScene.getDescriptorSet(), 0);

  const auto& geometryInstances = mScene.getScene().getGeometryInstances();
  const auto& meshes = mScene.getScene().getMeshes();

  struct {
    VkDeviceAddress sceneDataPtr;
    uint32_t geometryInstanceID;
  } push{};

  push.sceneDataPtr = mScene.getSceneDataDeviceAddress();

  for (size_t i = 0; i < geometryInstances.size(); ++i) {
    const auto& instance = geometryInstances.at(i);
    const auto& mesh = meshes.at(instance.meshID);

    push.geometryInstanceID = i;
    commandBuffer.pushConstants(
        *mPipelineLayout, sizeof(push), &push, VK_SHADER_STAGE_ALL);
    commandBuffer.drawIndexed(mesh.indexCount, 1, mesh.firstIndex, 0, 0);
  }

  commandBuffer.endRenderPass();
}

}  // namespace recore::passes
