#pragma once

#include <vulkan/vulkan.h>

#include <algorithm>
#include <format>
#include <stdexcept>
#include <vector>

namespace recore::vulkan {

enum class Vendor {
  NVIDIA = 0x10DE,
  AMD = 0x1002,
  INTEL = 0x8086,
  ARM = 0x13B5,
  MESA = VK_VENDOR_ID_MESA
};

static std::string to_string(Vendor vendor) {
  switch (vendor) {
    case Vendor::NVIDIA:
      return "NVIDIA";
    case Vendor::AMD:
      return "AMD";
    case Vendor::INTEL:
      return "INTEL";
    case Vendor::ARM:
      return "ARM";
    case Vendor::MESA:
      return "MESA";
    default:
      return "UNKNOWN Vendor";
  }
}

struct Version {
  Version() = default;

  Version(Vendor vendor, uint32_t version) {
    if (vendor == Vendor::NVIDIA) {
      major = (version >> 22U) & 0x3FFU;  // NOLINT
      minor = (version >> 14) & 0x0FFU;   // NOLINT
      patch = (version >> 6U) & 0x0FFU;   // NOLINT
    } else {
      major = VK_API_VERSION_MAJOR(version);
      minor = VK_API_VERSION_MINOR(version);
      patch = VK_API_VERSION_PATCH(version);
    }
  }

  uint32_t major{0};
  uint32_t minor{0};
  uint32_t patch{0};
};

static std::string to_string(Version version) {
  return std::format("{}.{}.{}", version.major, version.minor, version.patch);
}

static inline std::vector<const char*> strvec_to_cchar(
    const std::vector<std::string>& strvec) {
  std::vector<const char*> outvec{strvec.size()};
  std::transform(
      strvec.begin(), strvec.end(), outvec.begin(), [](const auto& str) {
        return str.c_str();
      });
  return outvec;
}

// Depth formats

static bool isDepthOnlyFormat(VkFormat format) {
  return format == VK_FORMAT_D32_SFLOAT || format == VK_FORMAT_D16_UNORM;
}

static bool isDepthStencilFormat(VkFormat format) {
  return format == VK_FORMAT_D32_SFLOAT_S8_UINT ||
         format == VK_FORMAT_D24_UNORM_S8_UINT ||
         format == VK_FORMAT_D16_UNORM_S8_UINT || isDepthOnlyFormat(format);
}

// String conversions from
// https://github.com/KhronosGroup/Vulkan-Samples/blob/main/framework/common/strings.cpp

static std::string to_string(VkResult result) {
  // NOLINTNEXTLINE
#define STR(r) \
  case VK_##r: \
    return #r
  switch (result) {
    STR(NOT_READY);
    STR(TIMEOUT);
    STR(EVENT_SET);
    STR(EVENT_RESET);
    STR(INCOMPLETE);
    STR(ERROR_OUT_OF_HOST_MEMORY);
    STR(ERROR_OUT_OF_DEVICE_MEMORY);
    STR(ERROR_INITIALIZATION_FAILED);
    STR(ERROR_DEVICE_LOST);
    STR(ERROR_MEMORY_MAP_FAILED);
    STR(ERROR_LAYER_NOT_PRESENT);
    STR(ERROR_EXTENSION_NOT_PRESENT);
    STR(ERROR_FEATURE_NOT_PRESENT);
    STR(ERROR_INCOMPATIBLE_DRIVER);
    STR(ERROR_TOO_MANY_OBJECTS);
    STR(ERROR_FORMAT_NOT_SUPPORTED);
    STR(ERROR_FRAGMENTED_POOL);
    STR(ERROR_UNKNOWN);
    STR(ERROR_OUT_OF_POOL_MEMORY);
    STR(ERROR_INVALID_EXTERNAL_HANDLE);
    STR(ERROR_FRAGMENTATION);
    STR(ERROR_INVALID_OPAQUE_CAPTURE_ADDRESS);
    STR(PIPELINE_COMPILE_REQUIRED);
    STR(ERROR_SURFACE_LOST_KHR);
    STR(ERROR_NATIVE_WINDOW_IN_USE_KHR);
    STR(SUBOPTIMAL_KHR);
    STR(ERROR_OUT_OF_DATE_KHR);
    STR(ERROR_INCOMPATIBLE_DISPLAY_KHR);
    STR(ERROR_VALIDATION_FAILED_EXT);
    STR(ERROR_INVALID_SHADER_NV);
#undef STR
    default:
      return "UNKNOWN_ERROR";
  }
}

class VulkanException : public std::runtime_error {
 public:
  explicit VulkanException(VkResult result, const std::string& message)
      : std::runtime_error{
            std::format("Vulkan Exception! Message = {}. VkResult = {}",
                        message,
                        to_string(result))} {}
};

constexpr void checkResult(VkResult result) {
  if (result != VK_SUCCESS) {
    throw recore::vulkan::VulkanException(
        result, std::format("checkResult({})", to_string(result)));
  }
}

constexpr uint32_t dispatchSize(uint32_t workgroupSize, uint32_t dataSize) {
  return (dataSize + workgroupSize - 1) / workgroupSize;
}

}  // namespace recore::vulkan
