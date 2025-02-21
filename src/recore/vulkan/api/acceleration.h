#pragma once

#include "buffer.h"
#include "command.h"
#include "device.h"

namespace recore::vulkan {

class AccelerationStructure : public Object<VkAccelerationStructureKHR> {
 public:
  struct Desc {
    const Device& device;
    VkAccelerationStructureTypeKHR type;
  };

  ~AccelerationStructure() override;

  [[nodiscard]] VkDeviceAddress getDeviceAddress() const {
    return mDeviceAddress;
  }

  void build(const CommandBuffer& commandBuffer);

 protected:
  explicit AccelerationStructure(const Desc& desc)
      : Object{desc.device}, mType{desc.type} {}

  void create();

  VkAccelerationStructureGeometryKHR mGeometry{};
  uint32_t mGeometryCount = 0;

 private:
  const VkAccelerationStructureTypeKHR mType;
  uPtr<Buffer> mASBuffer;
  uPtr<Buffer> mScratchBuffer;

  VkBuildAccelerationStructureFlagsKHR mBuildFlags =
      VK_BUILD_ACCELERATION_STRUCTURE_PREFER_FAST_TRACE_BIT_KHR |
      VK_BUILD_ACCELERATION_STRUCTURE_ALLOW_UPDATE_BIT_KHR;
  VkBuildAccelerationStructureModeKHR mBuildMode =
      VK_BUILD_ACCELERATION_STRUCTURE_MODE_BUILD_KHR;
  VkDeviceAddress mDeviceAddress{0};

  VkAccelerationStructureBuildGeometryInfoKHR mBuildGeometryInfo{};
};

class BLAS : public AccelerationStructure {
 public:
  struct Mesh {
    const Buffer& vertices;
    const Buffer& indices;
    const Buffer& transforms;
    uint32_t firstIndex = 0;
    uint32_t indexCount = 0;
    uint32_t vertexSize = 0;
    uint32_t transformOffset = 0;
  };

  struct Desc {
    const Device& device;
    const Mesh& mesh;
  };

  explicit BLAS(const Desc& desc);
};

class TLAS : public AccelerationStructure {
 public:
  struct Instance {
    const BLAS& blas;
    uint32_t instanceID;
    VkTransformMatrixKHR transform;
    VkGeometryInstanceFlagsKHR flags;
  };

  struct Desc {
    const Device& device;
    const std::vector<Instance>& instances;
  };

  explicit TLAS(const Desc& desc);

  void updateTransformMatrix(uint32_t instanceID,
                             const VkTransformMatrixKHR& transform);

 private:
  uPtr<Buffer> mInstances;
};

}  // namespace recore::vulkan
