#include "acceleration.h"

namespace recore::vulkan {
AccelerationStructure::~AccelerationStructure() {
  vkDestroyAccelerationStructureKHR(mDevice.vkHandle(), mHandle, nullptr);
}

void AccelerationStructure::build(const CommandBuffer& commandBuffer) {
  VkAccelerationStructureBuildRangeInfoKHR buildRangeInfo{};
  buildRangeInfo.primitiveCount = mGeometryCount;
  buildRangeInfo.primitiveOffset = 0;
  buildRangeInfo.firstVertex = 0;
  buildRangeInfo.transformOffset = 0;

  const auto* pBuildRangeInfo = &buildRangeInfo;

  vkCmdBuildAccelerationStructuresKHR(
      commandBuffer.vkHandle(), 1, &mBuildGeometryInfo, &pBuildRangeInfo);

  // From now on we can use the existing AS to update
  mBuildGeometryInfo.mode = VK_BUILD_ACCELERATION_STRUCTURE_MODE_UPDATE_KHR;
  mBuildGeometryInfo.srcAccelerationStructure = mHandle;
}

void AccelerationStructure::create() {
  VkAccelerationStructureBuildGeometryInfoKHR buildGeometryInfo{};
  buildGeometryInfo.sType =
      VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_BUILD_GEOMETRY_INFO_KHR;
  buildGeometryInfo.type = mType;
  buildGeometryInfo.flags = mBuildFlags;
  buildGeometryInfo.geometryCount = 1;
  buildGeometryInfo.pGeometries = &mGeometry;

  // https://registry.khronos.org/vulkan/specs/1.3-extensions/man/html/vkGetAccelerationStructureBuildSizesKHR.html
  // vkGetAccelerationStructureBuildSizesKHR ignores those:
  buildGeometryInfo.mode = VK_BUILD_ACCELERATION_STRUCTURE_MODE_BUILD_KHR;
  buildGeometryInfo.srcAccelerationStructure = VK_NULL_HANDLE;
  buildGeometryInfo.dstAccelerationStructure = VK_NULL_HANDLE;

  VkAccelerationStructureBuildSizesInfoKHR buildSizesInfo{};
  buildSizesInfo.sType =
      VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_BUILD_SIZES_INFO_KHR;
  vkGetAccelerationStructureBuildSizesKHR(
      mDevice.vkHandle(),
      VK_ACCELERATION_STRUCTURE_BUILD_TYPE_DEVICE_KHR,
      &buildGeometryInfo,
      &mGeometryCount,
      &buildSizesInfo);

  mASBuffer = makeUnique<Buffer>({
      .device = mDevice,
      .size = buildSizesInfo.accelerationStructureSize,
      .usage = VK_BUFFER_USAGE_ACCELERATION_STRUCTURE_STORAGE_BIT_KHR |
               VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT,
      .memoryUsage = VMA_MEMORY_USAGE_GPU_ONLY,
  });

  mScratchBuffer = makeUnique<Buffer>({
      .device = mDevice,
      // TODO: invesitage if max is necessary
      .size = std::max(buildSizesInfo.buildScratchSize,
                       buildSizesInfo.updateScratchSize),
      .usage = VK_BUFFER_USAGE_STORAGE_BUFFER_BIT |
               VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT,
      .memoryUsage = VMA_MEMORY_USAGE_GPU_ONLY,
      // TODO: .minAlignment maybe?
  });
  buildGeometryInfo.scratchData.deviceAddress =
      mScratchBuffer->getDeviceAddress();

  VkAccelerationStructureCreateInfoKHR accelerationStructureCreateInfo{};
  accelerationStructureCreateInfo.sType =
      VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_CREATE_INFO_KHR;
  accelerationStructureCreateInfo.buffer = mASBuffer->vkHandle();
  accelerationStructureCreateInfo.offset = 0;
  accelerationStructureCreateInfo.size =
      buildSizesInfo.accelerationStructureSize;
  accelerationStructureCreateInfo.type = mType;

  checkResult(vkCreateAccelerationStructureKHR(
      mDevice.vkHandle(), &accelerationStructureCreateInfo, nullptr, &mHandle));

  VkAccelerationStructureDeviceAddressInfoKHR
      accelerationStructureDeviceAddressInfo{};
  accelerationStructureDeviceAddressInfo.sType =
      VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_DEVICE_ADDRESS_INFO_KHR;
  accelerationStructureDeviceAddressInfo.accelerationStructure = mHandle;
  mDeviceAddress = vkGetAccelerationStructureDeviceAddressKHR(
      mDevice.vkHandle(), &accelerationStructureDeviceAddressInfo);

  mBuildGeometryInfo = buildGeometryInfo;
  mBuildGeometryInfo.dstAccelerationStructure = mHandle;
}

BLAS::BLAS(const Desc& desc)
    : AccelerationStructure{
          {.device = desc.device,
           .type = VK_ACCELERATION_STRUCTURE_TYPE_BOTTOM_LEVEL_KHR}} {
  VkAccelerationStructureGeometryKHR geometry{};
  geometry.sType = VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_GEOMETRY_KHR;
  geometry.geometryType = VK_GEOMETRY_TYPE_TRIANGLES_KHR;
  geometry.flags = VK_GEOMETRY_OPAQUE_BIT_KHR;

  const auto& mesh = desc.mesh;

  auto vertexAddress = mesh.vertices.getDeviceAddress();
  auto indexAddress = mesh.indices.getDeviceAddress();
  auto transformAddress = mesh.transforms.getDeviceAddress();

  // Offset index and transform buffer
  indexAddress += mesh.firstIndex * sizeof(uint32_t);
  transformAddress += mesh.transformOffset * sizeof(VkTransformMatrixKHR);

  auto maxVertex = mesh.vertices.getSize() / mesh.vertexSize;

  auto primitiveCount = mesh.indexCount / 3;

  VkAccelerationStructureGeometryTrianglesDataKHR triangles{};
  triangles.sType =
      VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_GEOMETRY_TRIANGLES_DATA_KHR;
  triangles.vertexFormat = VK_FORMAT_R32G32B32_SFLOAT;
  triangles.vertexData.deviceAddress = vertexAddress;
  triangles.vertexStride = mesh.vertexSize;
  triangles.maxVertex = maxVertex;
  triangles.indexType = VK_INDEX_TYPE_UINT32;
  triangles.indexData.deviceAddress = indexAddress;
  triangles.transformData.deviceAddress = transformAddress;

  geometry.geometry.triangles = triangles;

  mGeometry = geometry;
  mGeometryCount = primitiveCount;

  create();
}

TLAS::TLAS(const Desc& desc)
    : AccelerationStructure{
          {.device = desc.device,
           .type = VK_ACCELERATION_STRUCTURE_TYPE_TOP_LEVEL_KHR}} {
  mInstances = makeUnique<Buffer>({
      desc.device,
      desc.instances.size() * sizeof(VkAccelerationStructureInstanceKHR),
      VK_BUFFER_USAGE_ACCELERATION_STRUCTURE_BUILD_INPUT_READ_ONLY_BIT_KHR |
          VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT,
      VMA_MEMORY_USAGE_CPU_TO_GPU,
  });

  std::vector<VkAccelerationStructureInstanceKHR> instances;
  instances.reserve(desc.instances.size());
  for (const auto& instance : desc.instances) {
    VkAccelerationStructureInstanceKHR asInstance{};
    asInstance.transform = instance.transform;
    asInstance.instanceCustomIndex = instance.instanceID;
    asInstance.mask = 0xFF;
    asInstance.instanceShaderBindingTableRecordOffset = 0;
    asInstance.flags = instance.flags;
    asInstance.accelerationStructureReference =
        instance.blas.getDeviceAddress();

    instances.push_back(asInstance);
  }

  mInstances->upload(instances.data());

  VkAccelerationStructureGeometryKHR geometry{};
  geometry.sType = VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_GEOMETRY_KHR;
  geometry.geometryType = VK_GEOMETRY_TYPE_INSTANCES_KHR;
  geometry.flags = VK_GEOMETRY_OPAQUE_BIT_KHR;

  VkAccelerationStructureGeometryInstancesDataKHR geometryInstances{};
  geometryInstances.sType =
      VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_GEOMETRY_INSTANCES_DATA_KHR;
  geometryInstances.arrayOfPointers = VK_FALSE;
  geometryInstances.data.deviceAddress = mInstances->getDeviceAddress();

  geometry.geometry.instances = geometryInstances;

  mGeometry = geometry;
  mGeometryCount = static_cast<uint32_t>(instances.size());

  create();
}

void TLAS::updateTransformMatrix(uint32_t instanceID,
                                 const VkTransformMatrixKHR& transform) {
  mInstances->map();
  auto* instances = reinterpret_cast<VkAccelerationStructureInstanceKHR*>(
      mInstances->getMappedData());

  instances[instanceID].transform = transform;

  mInstances->flush();
  mInstances->unmap();
}
}  // namespace recore::vulkan
