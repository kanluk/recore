#include "distribution.h"

#include <recore/vulkan/debug.h>

namespace recore::passes {

constexpr auto kDistributionVertexShader =
    "recore/passes/debug/distribution.vert.glsl";

constexpr auto kDistributionFragmentShader =
    "recore/passes/debug/distribution.frag.glsl";

// Thanks to Addis :)
static glm::vec3 convertCubeUVToXYZ(uint32_t index, const glm::vec2& uv) {
  // convert range 0 to 1 to -1 to 1
  float uc = 2.0f * uv.x - 1.0f;
  float vc = 2.0f * uv.y - 1.0f;
  switch (index) {
    case 0:  // POSITIVE X
      return {1, vc, -uc};
    case 1:  // NEGATIVE X
      return {-1, vc, uc};
    case 2:  // POSITIVE Y
      return {uc, 1, -vc};
    case 3:  // NEGATIVE Y
      return {uc, -1, vc};
    case 4:  // POSITIVE Z
      return {uc, vc, 1};
    case 5:  // NEGATIVE Z
      return {-uc, vc, -1};
    default:
      return {0, 0, 0};
  }
}

void createSphere(uint32_t N,
                  std::vector<glm::vec3>& positions,
                  std::vector<uint32_t>& indices) {
  positions.resize(N * N * 6);
  indices.reserve(N * N * 6 * 4);

  auto index = [&](uint32_t i, uint32_t j, uint32_t f) -> uint32_t {
    return i * N + j + N * N * f;
  };

  auto get = [&](uint32_t i, uint32_t j, uint32_t f) -> glm::vec3& {
    return positions[i * N + j + N * N * f];
  };

  for (uint32_t f = 0; f < 6; f++) {
    for (uint32_t i = 0; i < N; i++) {
      for (uint32_t j = 0; j < N; j++) {
        glm::vec2 uv = glm::vec2(i, j) / static_cast<float>(N - 1);
        glm::vec3 dir = convertCubeUVToXYZ(f, uv);

        get(i, j, f) = glm::normalize(dir);
      }
    }

    for (uint32_t i = 0; i < N - 1; i++) {
      for (uint32_t j = 0; j < N - 1; j++) {
        indices.push_back(index(i, j, f));
        indices.push_back(index(i + 1, j, f));
        indices.push_back(index(i, j, f));
        indices.push_back(index(i, j + 1, f));
      }

      indices.push_back(index(N - 1, i, f));
      indices.push_back(index(N - 1, i + 1, f));
      indices.push_back(index(i, N - 1, f));
      indices.push_back(index(i + 1, N - 1, f));
    }
  }
}

DistributionVisualizationPass::DistributionVisualizationPass(
    const vulkan::Device& device, const scene::Camera& camera)
    : Pass{device}, mCamera{camera} {
  mRenderPass = makeUnique<vulkan::RenderPass>({
      .device = device,
      .attachments =
          {
              {
                  .format = VK_FORMAT_R8G8B8A8_UNORM,
                  .usage = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,
                  .initialLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,
                  .loadOp = VK_ATTACHMENT_LOAD_OP_LOAD,
              },
              {
                  .format = VK_FORMAT_D32_SFLOAT,
                  .usage = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL,
                  .initialLayout =
                      VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL,
                  .loadOp = VK_ATTACHMENT_LOAD_OP_LOAD,
              },
          },
  });

  // Generate sphere and upload to GPU
  std::vector<glm::vec3> positions;
  std::vector<uint32_t> indices;
  createSphere(10, positions, indices);
  positions.emplace_back(0.f, 0.f, 0.f);

  std::vector<uPtr<vulkan::Buffer>> garbage;
  mDevice.submitAndWait([&](const auto& commandBuffer) {
    auto genBuffer = [&]<typename T>(const std::vector<T>& data,
                                     VkBufferUsageFlags usage = 0) {
      VkDeviceSize size = sizeof(T) * data.size();
      auto buffer = makeUnique<vulkan::Buffer>({
          .device = mDevice,
          .size = 2 * size,
          .usage = VK_BUFFER_USAGE_STORAGE_BUFFER_BIT |
                   VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT |
                   VK_BUFFER_USAGE_TRANSFER_DST_BIT | usage,
          .memoryUsage = VMA_MEMORY_USAGE_GPU_ONLY,
      });

      auto staging = makeUnique<vulkan::Buffer>({
          .device = mDevice,
          .size = size,
          .usage = VK_BUFFER_USAGE_TRANSFER_SRC_BIT,
          .memoryUsage = VMA_MEMORY_USAGE_CPU_TO_GPU,
      });
      staging->upload(data.data());

      commandBuffer.copyBufferToBuffer(*staging, *buffer);
      commandBuffer.copyBufferToBuffer(
          *staging, *buffer, {.dstOffset = staging->getSize()});

      garbage.push_back(std::move(staging));

      return buffer;
    };

    auto genIndexBuffer = [&]<typename T>(

                              const std::vector<T>& data,
                              VkBufferUsageFlags usage = 0) {
      VkDeviceSize size = sizeof(T) * data.size();
      auto buffer = makeUnique<vulkan::Buffer>({
          .device = mDevice,
          .size = size,
          .usage = VK_BUFFER_USAGE_STORAGE_BUFFER_BIT |
                   VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT |
                   VK_BUFFER_USAGE_TRANSFER_DST_BIT | usage,
          .memoryUsage = VMA_MEMORY_USAGE_GPU_ONLY,
      });

      auto staging = makeUnique<vulkan::Buffer>({
          .device = mDevice,
          .size = size,
          .usage = VK_BUFFER_USAGE_TRANSFER_SRC_BIT,
          .memoryUsage = VMA_MEMORY_USAGE_CPU_TO_GPU,
      });
      staging->upload(data.data());

      commandBuffer.copyBufferToBuffer(*staging, *buffer);

      garbage.push_back(std::move(staging));

      return buffer;
    };

    mVertexBuffer = genBuffer(positions, VK_BUFFER_USAGE_VERTEX_BUFFER_BIT);
    mIndexBuffer = genIndexBuffer(indices, VK_BUFFER_USAGE_INDEX_BUFFER_BIT);
  });
}

void DistributionVisualizationPass::reloadShaders(
    vulkan::ShaderLibrary& shaderLibrary) {
  mPipelineLayout = makeUnique<vulkan::PipelineLayout>({
      .device = mDevice,
      .descriptorSetLayouts = {},
      .pushConstants = {{
          .stageFlags = VK_SHADER_STAGE_VERTEX_BIT,
          .size = sizeof(mPush),
      }},
  });

  mPipeline = makeUnique<vulkan::RasterPipeline>({
      .device = mDevice,
      .layout = *mPipelineLayout,
      .renderPass = *mRenderPass,
      .state =
          {
              .vertexBindingDescriptions = {VkVertexInputBindingDescription{
                  0, sizeof(glm::vec3), VK_VERTEX_INPUT_RATE_VERTEX}},
              .vertexAttributeDescriptions = {VkVertexInputAttributeDescription{
                  0, 0, VK_FORMAT_R32G32B32_SFLOAT, 0}},
              .topology = VK_PRIMITIVE_TOPOLOGY_LINE_LIST,
              .depthTest = true,
              .depthWrite = false,
          },
      .shaders =
          {
              shaderLibrary.loadShader(kDistributionVertexShader),
              shaderLibrary.loadShader(kDistributionFragmentShader),
          },
  });
}

void DistributionVisualizationPass::execute(
    const vulkan::CommandBuffer& commandBuffer,
    vulkan::RenderFrame& currentFrame) {
  RECORE_DEBUG_SCOPE(commandBuffer, "DistributionVisualizationPass::draw");

  commandBuffer.beginRenderPass(
      *mRenderPass,
      *mFramebuffer,
      {{0.f, 0.f, 0.f, 1.f}, {.depthStencil = {1.f, 0}}});

  commandBuffer.bindPipeline(*mPipeline);

  mPush.viewProjection = mCamera.getViewProjection();
  commandBuffer.pushConstants(
      *mPipelineLayout, sizeof(mPush), &mPush, VK_SHADER_STAGE_VERTEX_BIT);

  commandBuffer.bindVertexBuffer(*mVertexBuffer);
  commandBuffer.bindIndexBuffer(*mIndexBuffer);

  commandBuffer.drawIndexed(mIndexBuffer->getSize() / sizeof(uint32_t));

  commandBuffer.endRenderPass();
}

}  // namespace recore::passes
