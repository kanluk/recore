#pragma once

#include "descriptor.h"
#include "device.h"
#include "renderpass.h"

namespace recore::vulkan {

constexpr uint32_t MAX_PUSH_CONSTANT_SIZE = 128;

class Shader : public Object<VkShaderModule> {
 public:
  struct Desc {
    const Device& device;
    VkShaderStageFlagBits stage;
    std::vector<uint32_t> spriv;
  };

  explicit Shader(const Desc& desc);
  ~Shader() override;

  [[nodiscard]] VkPipelineShaderStageCreateInfo getStageInfo() const;

 private:
  VkShaderStageFlagBits mStage{};
  std::vector<uint32_t> mSpirv;
};

class PipelineLayout : public Object<VkPipelineLayout> {
 public:
  struct Desc {
    const Device& device;
    std::vector<const DescriptorSetLayout*> descriptorSetLayouts;
    const std::vector<VkPushConstantRange>& pushConstants;
  };

  explicit PipelineLayout(const Desc& desc);
  ~PipelineLayout() override;
};

class Pipeline : public Object<VkPipeline> {
 public:
  Pipeline(const Device& device,
           const PipelineLayout& layout,
           VkPipelineBindPoint bindPoint);
  ~Pipeline() override;

  [[nodiscard]] const PipelineLayout& getLayout() const { return mLayout; }

  [[nodiscard]] VkPipelineBindPoint getBindPoint() const { return mBindPoint; }

 protected:
  const PipelineLayout& mLayout;
  const VkPipelineBindPoint mBindPoint{};
};

class RasterPipeline : public Pipeline {
 public:
  struct State {
    // Input vertex format (bindings and attributes).
    std::vector<VkVertexInputBindingDescription> vertexBindingDescriptions;
    std::vector<VkVertexInputAttributeDescription> vertexAttributeDescriptions;

    // Triangle topology & primitive restart (e.g. for triangle strip).
    VkPrimitiveTopology topology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST;
    bool primitiveRestart = false;

    // Viewport and scissor rectangle.
    VkViewport viewport = {.x = 0.f,
                           .y = 0.f,
                           .width = 0.f,
                           .height = 0.f,
                           .minDepth = 0.f,
                           .maxDepth = 0.f};
    VkRect2D scissor = {.offset = {0, 0}, .extent = {0, 0}};

    // Rasterization state.
    bool depthClamp = false;
    bool rasterizerDiscard = false;
    VkPolygonMode polygonMode = VK_POLYGON_MODE_FILL;
    VkCullModeFlags cullMode = VK_CULL_MODE_NONE;
    VkFrontFace frontFace = VK_FRONT_FACE_COUNTER_CLOCKWISE;
    bool depthBias = false;
    float depthBiasConstantFactor = 0.f;
    float depthBiasClamp = 0.f;
    float depthBiasSlopeFactor = 0.f;
    float lineWidth = 1.f;

    // Multisample state.
    VkSampleCountFlagBits sampleCount = VK_SAMPLE_COUNT_1_BIT;
    bool sampleShading = false;
    float minSampleShading = 1.f;
    bool alphaToCoverage = false;
    bool alphaToOne = false;

    // Depth & stencil test.
    bool depthTest = false;
    bool depthWrite = false;
    VkCompareOp depthCompareOp = VK_COMPARE_OP_LESS;
    bool depthBoundsTest = false;
    bool stencilTest = false;
    VkStencilOpState frontStencil{};
    VkStencilOpState backStencil{};
    float minDepthBounds = 0.f;
    float maxDepthBounds = 1.f;

    // Blending (operation and factors) for EACH color attachment.
    VkPipelineColorBlendAttachmentState colorBlendAttachmentState = {
        .blendEnable = VK_FALSE,
        .srcColorBlendFactor = VK_BLEND_FACTOR_ONE,
        .dstColorBlendFactor = VK_BLEND_FACTOR_ZERO,
        .colorBlendOp = VK_BLEND_OP_ADD,
        .srcAlphaBlendFactor = VK_BLEND_FACTOR_ONE,
        .dstAlphaBlendFactor = VK_BLEND_FACTOR_ZERO,
        .alphaBlendOp = VK_BLEND_OP_ADD,
        .colorWriteMask = VK_COLOR_COMPONENT_R_BIT | VK_COLOR_COMPONENT_G_BIT |
                          VK_COLOR_COMPONENT_B_BIT | VK_COLOR_COMPONENT_A_BIT};
    uint32_t colorAttachmentCount = 1;

    // Dynamic pipeline states that can be set by a command buffer.
    std::vector<VkDynamicState> dynamicEnabled = {VK_DYNAMIC_STATE_VIEWPORT,
                                                  VK_DYNAMIC_STATE_SCISSOR};
  };

  struct Desc {
    const Device& device;
    const PipelineLayout& layout;
    const RenderPass& renderPass;
    const State& state;
    std::vector<const Shader*> shaders;
  };

  explicit RasterPipeline(const Desc& desc);
};

class ComputePipeline : public Pipeline {
 public:
  struct Desc {
    const Device& device;
    const PipelineLayout& layout;
    const Shader& shader;
  };

  explicit ComputePipeline(const Desc& desc)
      : Pipeline{desc.device, desc.layout, VK_PIPELINE_BIND_POINT_COMPUTE} {
    VkComputePipelineCreateInfo pipelineInfo{};
    pipelineInfo.sType = VK_STRUCTURE_TYPE_COMPUTE_PIPELINE_CREATE_INFO;
    pipelineInfo.stage = desc.shader.getStageInfo();
    pipelineInfo.layout = mLayout.vkHandle();

    pipelineInfo.basePipelineHandle = VK_NULL_HANDLE;
    pipelineInfo.basePipelineIndex = -1;

    checkResult(vkCreateComputePipelines(mDevice.vkHandle(),
                                         VK_NULL_HANDLE,
                                         1,
                                         &pipelineInfo,
                                         nullptr,
                                         &mHandle));
  }
};

class RayTracingPipeline {};

}  // namespace recore::vulkan
