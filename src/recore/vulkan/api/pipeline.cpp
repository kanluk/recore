#include "pipeline.h"

#include <algorithm>

namespace recore::vulkan {

Shader::Shader(const Desc& desc)
    : Object{desc.device}, mStage{desc.stage}, mSpirv{desc.spriv} {
  VkShaderModuleCreateInfo moduleInfo{};
  moduleInfo.sType = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO;
  moduleInfo.codeSize = mSpirv.size() * sizeof(uint32_t);
  moduleInfo.pCode = mSpirv.data();

  checkResult(
      vkCreateShaderModule(mDevice.vkHandle(), &moduleInfo, nullptr, &mHandle));
}

Shader::~Shader() {
  vkDestroyShaderModule(mDevice.vkHandle(), mHandle, nullptr);
}

VkPipelineShaderStageCreateInfo Shader::getStageInfo() const {
  VkPipelineShaderStageCreateInfo info{};
  info.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
  info.stage = mStage;
  info.module = mHandle;
  info.pName = "main";
  return info;
}

PipelineLayout::PipelineLayout(const Desc& desc) : Object{desc.device} {
  auto vkDescriptorSetLayouts =
      rstd::transform<const DescriptorSetLayout*, VkDescriptorSetLayout>(
          desc.descriptorSetLayouts, [](const auto& descriptorSetLayout) {
            return descriptorSetLayout->vkHandle();
          });

  VkPipelineLayoutCreateInfo layoutInfo{};
  layoutInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
  layoutInfo.setLayoutCount = static_cast<uint32_t>(
      vkDescriptorSetLayouts.size());
  layoutInfo.pSetLayouts = vkDescriptorSetLayouts.data();
  layoutInfo.pushConstantRangeCount = static_cast<uint32_t>(
      desc.pushConstants.size());
  layoutInfo.pPushConstantRanges = desc.pushConstants.data();

  checkResult(vkCreatePipelineLayout(
      mDevice.vkHandle(), &layoutInfo, nullptr, &mHandle));
}

PipelineLayout::~PipelineLayout() {
  vkDestroyPipelineLayout(mDevice.vkHandle(), mHandle, nullptr);
}

Pipeline::Pipeline(const Device& device,
                   const PipelineLayout& layout,
                   VkPipelineBindPoint bindPoint)
    : Object{device}, mLayout{layout}, mBindPoint{bindPoint} {}

Pipeline::~Pipeline() {
  vkDestroyPipeline(mDevice.vkHandle(), mHandle, nullptr);
}

RasterPipeline::RasterPipeline(const Desc& desc)
    : Pipeline{desc.device, desc.layout, VK_PIPELINE_BIND_POINT_GRAPHICS} {
  std::vector<VkPipelineShaderStageCreateInfo> shaderStages{
      desc.shaders.size()};
  std::transform(desc.shaders.begin(),
                 desc.shaders.end(),
                 shaderStages.begin(),
                 [](const auto& shader) { return shader->getStageInfo(); });

  const auto& state = desc.state;

  VkPipelineVertexInputStateCreateInfo vertexInputState = {
      .sType = VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO,
      .vertexBindingDescriptionCount = static_cast<uint32_t>(
          state.vertexBindingDescriptions.size()),
      .pVertexBindingDescriptions = state.vertexBindingDescriptions.data(),
      .vertexAttributeDescriptionCount = static_cast<uint32_t>(
          state.vertexAttributeDescriptions.size()),
      .pVertexAttributeDescriptions = state.vertexAttributeDescriptions.data()};

  VkPipelineInputAssemblyStateCreateInfo inputAssemblyState = {
      .sType = VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO,
      .topology = state.topology,
      .primitiveRestartEnable = static_cast<VkBool32>(state.primitiveRestart)};

  VkPipelineTessellationStateCreateInfo tessellationState = {
      .sType = VK_STRUCTURE_TYPE_PIPELINE_TESSELLATION_STATE_CREATE_INFO,
      .patchControlPoints = 0};

  VkPipelineViewportStateCreateInfo viewportState = {
      .sType = VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO,
      .viewportCount = 1,
      .pViewports = nullptr,
      .scissorCount = 1,
      .pScissors = nullptr};
  if (!std::ranges::contains(state.dynamicEnabled, VK_DYNAMIC_STATE_VIEWPORT)) {
    viewportState.pViewports = &state.viewport;
  }
  if (!std::ranges::contains(state.dynamicEnabled, VK_DYNAMIC_STATE_SCISSOR)) {
    viewportState.pScissors = &state.scissor;
  }

  VkPipelineRasterizationStateCreateInfo rasterizationState = {
      .sType = VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO,
      .depthClampEnable = static_cast<VkBool32>(state.depthClamp),
      .rasterizerDiscardEnable = static_cast<VkBool32>(state.rasterizerDiscard),
      .polygonMode = state.polygonMode,
      .cullMode = state.cullMode,
      .frontFace = state.frontFace,
      .depthBiasEnable = static_cast<VkBool32>(state.depthBias),
      .depthBiasConstantFactor = state.depthBiasConstantFactor,
      .depthBiasClamp = state.depthBiasClamp,
      .depthBiasSlopeFactor = state.depthBiasSlopeFactor,
      .lineWidth = state.lineWidth};

  VkPipelineMultisampleStateCreateInfo multisampleState = {
      .sType = VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO,
      .rasterizationSamples = state.sampleCount,
      .sampleShadingEnable = static_cast<VkBool32>(state.sampleShading),
      .minSampleShading = state.minSampleShading,
      .pSampleMask = nullptr,
      .alphaToCoverageEnable = static_cast<VkBool32>(state.alphaToCoverage),
      .alphaToOneEnable = static_cast<VkBool32>(state.alphaToOne)};

  VkPipelineDepthStencilStateCreateInfo depthStencilState = {
      .sType = VK_STRUCTURE_TYPE_PIPELINE_DEPTH_STENCIL_STATE_CREATE_INFO,
      .depthTestEnable = static_cast<VkBool32>(state.depthTest),
      .depthWriteEnable = static_cast<VkBool32>(state.depthWrite),
      .depthCompareOp = state.depthCompareOp,
      .depthBoundsTestEnable = static_cast<VkBool32>(state.depthBoundsTest),
      .stencilTestEnable = static_cast<VkBool32>(state.stencilTest),
      .front = state.frontStencil,
      .back = state.backStencil,
      .minDepthBounds = state.minDepthBounds,
      .maxDepthBounds = state.maxDepthBounds};

  std::vector<VkPipelineColorBlendAttachmentState> colorBlendAttachments(
      state.colorAttachmentCount, state.colorBlendAttachmentState);
  VkPipelineColorBlendStateCreateInfo colorBlendState = {
      .sType = VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO,
      .logicOpEnable = VK_FALSE,
      .logicOp = VK_LOGIC_OP_COPY,
      .attachmentCount = static_cast<uint32_t>(colorBlendAttachments.size()),
      .pAttachments = colorBlendAttachments.data(),
      .blendConstants = {0.f, 0.f, 0.f, 0.f}};

  VkPipelineDynamicStateCreateInfo dynamicState{
      .sType = VK_STRUCTURE_TYPE_PIPELINE_DYNAMIC_STATE_CREATE_INFO,
      .dynamicStateCount = static_cast<uint32_t>(state.dynamicEnabled.size()),
      .pDynamicStates = state.dynamicEnabled.data()};

  VkGraphicsPipelineCreateInfo pipelineInfo{};
  pipelineInfo.sType = VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO;
  pipelineInfo.stageCount = static_cast<uint32_t>(shaderStages.size());
  pipelineInfo.pStages = shaderStages.data();
  pipelineInfo.pVertexInputState = &vertexInputState;
  pipelineInfo.pInputAssemblyState = &inputAssemblyState;
  pipelineInfo.pTessellationState = &tessellationState;
  pipelineInfo.pViewportState = &viewportState;
  pipelineInfo.pRasterizationState = &rasterizationState;
  pipelineInfo.pMultisampleState = &multisampleState;
  pipelineInfo.pDepthStencilState = &depthStencilState;
  pipelineInfo.pColorBlendState = &colorBlendState;
  pipelineInfo.pDynamicState = &dynamicState;

  pipelineInfo.layout = mLayout.vkHandle();
  pipelineInfo.renderPass = desc.renderPass.vkHandle();

  // Ignore subpasses & pipeline derivatives for now.
  pipelineInfo.subpass = 0;
  pipelineInfo.basePipelineHandle = VK_NULL_HANDLE;
  pipelineInfo.basePipelineIndex = -1;

  checkResult(vkCreateGraphicsPipelines(
      mDevice.vkHandle(), VK_NULL_HANDLE, 1, &pipelineInfo, nullptr, &mHandle));
}

}  // namespace recore::vulkan
