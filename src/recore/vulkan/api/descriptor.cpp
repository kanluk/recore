#include "descriptor.h"

namespace recore::vulkan {

DescriptorSetLayout::DescriptorSetLayout(const Desc& desc)
    : Object{desc.device} {
  auto bindings = rstd::transform<DescriptorSetLayout::BindingDesc,
                                  VkDescriptorSetLayoutBinding>(
      desc.bindings, [](const auto& binding) -> VkDescriptorSetLayoutBinding {
        return binding.binding;
      });
  auto bindingFlags = rstd::transform<DescriptorSetLayout::BindingDesc,
                                      VkDescriptorBindingFlags>(
      desc.bindings, [](const auto& binding) { return binding.flags; });

  VkDescriptorSetLayoutCreateInfo layoutInfo{};
  layoutInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;
  layoutInfo.bindingCount = static_cast<uint32_t>(bindings.size());
  layoutInfo.pBindings = bindings.data();

  VkDescriptorSetLayoutBindingFlagsCreateInfo bindingFlagsInfo{};
  bindingFlagsInfo.sType =
      VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_BINDING_FLAGS_CREATE_INFO;
  bindingFlagsInfo.bindingCount = static_cast<uint32_t>(bindingFlags.size());
  bindingFlagsInfo.pBindingFlags = bindingFlags.data();
  layoutInfo.pNext = &bindingFlagsInfo;

  checkResult(vkCreateDescriptorSetLayout(
      mDevice.vkHandle(), &layoutInfo, nullptr, &mHandle));

  for (const auto& binding : bindings) {
    if (mBindings.contains(binding.binding)) {
      throw std::runtime_error("Duplicate bindings in descriptor set layout.");
    }
    mBindings[binding.binding] = binding;
  }
}

DescriptorSetLayout::~DescriptorSetLayout() {
  vkDestroyDescriptorSetLayout(mDevice.vkHandle(), mHandle, nullptr);
}

DescriptorPool::DescriptorPool(const Desc& desc) : Object{desc.device} {
  auto poolSizes = desc.getPoolSizes();

  VkDescriptorPoolCreateInfo poolInfo{};
  poolInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO;
  poolInfo.flags = VK_DESCRIPTOR_POOL_CREATE_FREE_DESCRIPTOR_SET_BIT;
  poolInfo.maxSets = desc.maxSets;
  poolInfo.poolSizeCount = static_cast<uint32_t>(poolSizes.size());
  poolInfo.pPoolSizes = poolSizes.data();

  checkResult(
      vkCreateDescriptorPool(mDevice.vkHandle(), &poolInfo, nullptr, &mHandle));
}

DescriptorPool::~DescriptorPool() {
  vkDestroyDescriptorPool(mDevice.vkHandle(), mHandle, nullptr);
}

DescriptorSet::DescriptorSet(const Desc& desc)
    : Object{desc.device}, mPool{desc.pool}, mLayout{desc.layout} {
  VkDescriptorSetAllocateInfo setInfo{};
  setInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
  setInfo.descriptorPool = mPool.vkHandle();
  setInfo.descriptorSetCount = 1;
  setInfo.pSetLayouts = mLayout.vkPtr();

  checkResult(vkAllocateDescriptorSets(mDevice.vkHandle(), &setInfo, &mHandle));
}

void DescriptorSet::update(const Resources& resources) const {
  std::vector<VkWriteDescriptorSet> writeSets;

  // Write to buffer bindings
  for (const auto& [bindingIdx, index] : resources.buffers) {
    const auto& binding = mLayout.getBinding(bindingIdx);
    for (const auto& [index, buffer] : index) {
      VkWriteDescriptorSet write{};
      write.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
      write.dstSet = mHandle;
      write.dstBinding = binding.binding;
      write.dstArrayElement = index;
      write.descriptorCount = 1;
      write.descriptorType = binding.descriptorType;
      write.pImageInfo = nullptr;
      write.pBufferInfo = &buffer;
      write.pTexelBufferView = nullptr;

      writeSets.push_back(write);
    }
  }

  // Write to image bindings
  for (const auto& [bindingIdx, index] : resources.images) {
    const auto& binding = mLayout.getBinding(bindingIdx);
    for (const auto& [index, image] : index) {
      VkWriteDescriptorSet write{};
      write.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
      write.dstSet = mHandle;
      write.dstBinding = binding.binding;
      write.dstArrayElement = index;
      write.descriptorCount = 1;
      write.descriptorType = binding.descriptorType;
      write.pImageInfo = &image;
      write.pBufferInfo = nullptr;
      write.pTexelBufferView = nullptr;

      writeSets.push_back(write);
    }
  }

  // TODO: asserts only a single acceleration structure
  // Write to acceleration structure bindings
  VkWriteDescriptorSetAccelerationStructureKHR writeAccelerationStructure{};
  for (const auto& [bindingIdx, index] : resources.accelerationStructures) {
    const auto& binding = mLayout.getBinding(bindingIdx);
    for (const auto& [index, accelerationStructure] : index) {
      VkWriteDescriptorSet write{};
      write.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
      write.dstSet = mHandle;
      write.dstBinding = binding.binding;
      write.dstArrayElement = index;
      write.descriptorCount = 1;
      write.descriptorType = binding.descriptorType;
      write.pImageInfo = nullptr;
      write.pBufferInfo = nullptr;
      write.pTexelBufferView = nullptr;

      writeAccelerationStructure.sType =
          VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET_ACCELERATION_STRUCTURE_KHR;
      writeAccelerationStructure.accelerationStructureCount = 1;
      writeAccelerationStructure.pAccelerationStructures =
          &accelerationStructure;
      write.pNext = &writeAccelerationStructure;

      writeSets.push_back(write);
    }
  }

  vkUpdateDescriptorSets(mDevice.vkHandle(),
                         static_cast<uint32_t>(writeSets.size()),
                         writeSets.data(),
                         0,
                         nullptr);
}

}  // namespace recore::vulkan
