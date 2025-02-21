#pragma once

#include <recore/vulkan/api/command.h>

#include <recore/vulkan/api/object.h>

namespace recore::vulkan {

class ScopedDebugMarker {
 public:
  ScopedDebugMarker(const CommandBuffer& commandBuffer, const std::string& name)
      : mCommandBuffer{commandBuffer} {
    if (mCommandBuffer.getDevice().getInstance().isValidationEnabled()) {
      VkDebugUtilsLabelEXT markerInfo{
          .sType = VK_STRUCTURE_TYPE_DEBUG_UTILS_LABEL_EXT,
          .pNext = nullptr,
          .pLabelName = name.c_str(),
          .color = {1.0, 0.0, 0.0, 1.0},
      };
      vkCmdBeginDebugUtilsLabelEXT(commandBuffer.vkHandle(), &markerInfo);
    }
  }

  ~ScopedDebugMarker() {
    if (mCommandBuffer.getDevice().getInstance().isValidationEnabled()) {
      vkCmdEndDebugUtilsLabelEXT(mCommandBuffer.vkHandle());
    }
  }

 private:
  const CommandBuffer& mCommandBuffer;
};

namespace debug {

template <typename T>
void setName(const Object<T>& object, const std::string& name) {
  if (!object.getDevice().getInstance().isValidationEnabled()) {
    return;
  }

  VkObjectType objectType = VK_OBJECT_TYPE_UNKNOWN;

  if constexpr (std::is_same_v<T, VkBuffer>) {
    objectType = VK_OBJECT_TYPE_BUFFER;
  }

  auto objectHandle = static_cast<uint64_t>(
      *reinterpret_cast<const uint64_t*>(object.vkPtr()));

  VkDebugUtilsObjectNameInfoEXT nameInfo{
      .sType = VK_STRUCTURE_TYPE_DEBUG_UTILS_OBJECT_NAME_INFO_EXT,
      .pNext = nullptr,
      .objectType = objectType,
      .objectHandle = objectHandle,
      .pObjectName = name.c_str(),
  };

  vkSetDebugUtilsObjectNameEXT(object.getDevice().vkHandle(), &nameInfo);
}

}  // namespace debug

}  // namespace recore::vulkan

// NOLINTNEXTLINE(cppcoreguidelines-macro-usage)
#define RECORE_DEBUG_SCOPE(COMMAND_BUFFER, NAME) \
  recore::vulkan::ScopedDebugMarker ___debugScope{COMMAND_BUFFER, NAME};
