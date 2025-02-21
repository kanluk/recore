#pragma once

#include "device.h"

#include <map>

namespace recore::vulkan {

class DescriptorSetLayout : public Object<VkDescriptorSetLayout> {
 public:
  struct BindingDesc {
    VkDescriptorSetLayoutBinding binding{};
    VkDescriptorBindingFlags flags = 0;
  };

  struct Desc {
    const Device& device;
    const std::vector<BindingDesc>& bindings;
  };

  explicit DescriptorSetLayout(const Desc& desc);
  ~DescriptorSetLayout() override;

  [[nodiscard]] const VkDescriptorSetLayoutBinding& getBinding(
      uint32_t binding) const {
    return mBindings.at(binding);
  }

 private:
  std::map<uint32_t, VkDescriptorSetLayoutBinding> mBindings;
};

class DescriptorPool : public Object<VkDescriptorPool> {
 public:
  struct Desc {
    const Device& device;
    std::vector<VkDescriptorPoolSize> poolSizes = {
        {VK_DESCRIPTOR_TYPE_SAMPLER, 1},
        {VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 1},
        {VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE, 1},
        {VK_DESCRIPTOR_TYPE_STORAGE_IMAGE, 1},
        {VK_DESCRIPTOR_TYPE_UNIFORM_TEXEL_BUFFER, 1},
        {VK_DESCRIPTOR_TYPE_STORAGE_TEXEL_BUFFER, 1},
        {VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, 1},
        {VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, 1},
        {VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER_DYNAMIC, 1},
        {VK_DESCRIPTOR_TYPE_STORAGE_BUFFER_DYNAMIC, 1},
        {VK_DESCRIPTOR_TYPE_INPUT_ATTACHMENT, 1}};
    uint32_t maxSets = 1;
    uint32_t multiplier = 1;
    bool perSet = false;

    [[nodiscard]] std::vector<VkDescriptorPoolSize> getPoolSizes() const {
      uint32_t finalMultiplier = multiplier * (perSet ? maxSets : 1);

      std::vector<VkDescriptorPoolSize> sizes(poolSizes.size());
      std::transform(
          poolSizes.begin(),
          poolSizes.end(),
          sizes.begin(),
          [finalMultiplier](
              const VkDescriptorPoolSize& poolSize) -> VkDescriptorPoolSize {
            return {poolSize.type, poolSize.descriptorCount * finalMultiplier};
          });
      return sizes;
    }
  };

  explicit DescriptorPool(const Desc& desc);
  ~DescriptorPool() override;
};

// For a set: binding -> array index -> resource
template <typename Resource>
using DescriptorMap = std::map<uint32_t, std::map<uint32_t, Resource>>;

class DescriptorSet : public Object<VkDescriptorSet> {
 public:
  struct Desc {
    const Device& device;
    const DescriptorPool& pool;
    const DescriptorSetLayout& layout;
  };

  struct Resources {
    DescriptorMap<VkDescriptorBufferInfo> buffers;
    DescriptorMap<VkDescriptorImageInfo> images;
    DescriptorMap<VkAccelerationStructureKHR> accelerationStructures;
  };

  explicit DescriptorSet(const Desc& desc);
  ~DescriptorSet() override = default;  // Handled by pool

  void update(const Resources& resources) const;

 private:
  const DescriptorPool& mPool;
  const DescriptorSetLayout& mLayout;
};

}  // namespace recore::vulkan
