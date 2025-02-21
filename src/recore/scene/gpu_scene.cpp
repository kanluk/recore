#include "gpu_scene.h"

#include <recore/vulkan/api/command.h>

namespace recore::scene {

static VkTransformMatrixKHR glmToVulkanTransform(const glm::mat4& T) {
  VkTransformMatrixKHR transformMatrix = {T[0][0],
                                          T[1][0],
                                          T[2][0],
                                          T[3][0],
                                          T[0][1],
                                          T[1][1],
                                          T[2][1],
                                          T[3][1],
                                          T[0][2],
                                          T[1][2],
                                          T[2][2],
                                          T[3][2]};
  return transformMatrix;
}

GPUScene::GPUScene(const vulkan::Device& device,
                   const Scene& scene,
                   bool enableRayTracing)
    : mDevice{device}, mScene{scene}, mEnableRayTracing{enableRayTracing} {}

void GPUScene::upload() {
  const auto& queue = mDevice.getGraphicsQueue();
  vulkan::CommandPool commandPool{{
      .device = mDevice,
      .queueFamilyIndex = queue.getFamilyIndex(),
  }};

  vulkan::CommandBuffer commandBuffer{{
      .device = mDevice,
      .commandPool = commandPool,
  }};

  std::vector<uPtr<vulkan::Buffer>> garbage;

  auto genBuffer = [&]<typename T>(const std::vector<T>& data,
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

  auto imageFormat = [](Scene::Texture::Format format) {
    switch (format) {
      case Scene::Texture::Format::SRGB:
        return VK_FORMAT_R8G8B8A8_SRGB;
      case Scene::Texture::Format::UNORM:
      default:
        return VK_FORMAT_R8G8B8A8_UNORM;
    }
  };

  // Start uploading

  commandBuffer.begin();

  // Buffers
  mBuffers.vertices = genBuffer(
      mScene.getVertices(),
      VK_BUFFER_USAGE_VERTEX_BUFFER_BIT |
          (mEnableRayTracing
               ? VK_BUFFER_USAGE_ACCELERATION_STRUCTURE_BUILD_INPUT_READ_ONLY_BIT_KHR
               : 0));
  mBuffers.indices = genBuffer(
      mScene.getIndices(),
      VK_BUFFER_USAGE_INDEX_BUFFER_BIT |
          (mEnableRayTracing
               ? VK_BUFFER_USAGE_ACCELERATION_STRUCTURE_BUILD_INPUT_READ_ONLY_BIT_KHR
               : 0));
  mBuffers.meshes = genBuffer(mScene.getMeshes());
  mBuffers.geometryInstances = genBuffer(mScene.getGeometryInstances());
  mBuffers.modelMatrices = genBuffer(mScene.getModelMatrices());
  mBuffers.materials = genBuffer(mScene.getMaterials());
  mBuffers.lights = genBuffer(mScene.getLights());

  mSceneData.vertices = mBuffers.vertices->getDeviceAddress();
  mSceneData.indices = mBuffers.indices->getDeviceAddress();
  mSceneData.meshes = mBuffers.meshes->getDeviceAddress();
  mSceneData.geometryInstances = mBuffers.geometryInstances->getDeviceAddress();
  mSceneData.modelMatrices = mBuffers.modelMatrices->getDeviceAddress();
  mSceneData.materials = mBuffers.materials->getDeviceAddress();

  mSceneData.lightCount = mScene.getLights().size();
  mSceneData.lights = mBuffers.lights->getDeviceAddress();

  mSceneData.camera.viewProjection = mScene.getCamera().getViewProjection();
  mSceneData.camera.prevViewProjection = mSceneData.camera.viewProjection;

  mBuffers.sceneData = genBuffer(std::vector{mSceneData});

  // Images
  const auto& textures = mScene.getTextures();
  mImages.reserve(textures.size());
  for (const auto& texture : textures) {
    auto image = makeUnique<vulkan::Image>({
        .device = mDevice,
        .format = imageFormat(texture.format),
        .extent = {texture.width, texture.height, 1},
        .usage = VK_IMAGE_USAGE_SAMPLED_BIT | VK_IMAGE_USAGE_TRANSFER_DST_BIT |
                 VK_IMAGE_USAGE_TRANSFER_SRC_BIT,
        .memoryUsage = VMA_MEMORY_USAGE_GPU_ONLY,
    });

    auto stagingBuffer = makeUnique<vulkan::Buffer>({
        .device = mDevice,
        .size = texture.image.size() * sizeof(uint8_t),
        .usage = VK_BUFFER_USAGE_TRANSFER_SRC_BIT,
        .memoryUsage = VMA_MEMORY_USAGE_CPU_TO_GPU,
    });
    stagingBuffer->upload(texture.image.data());

    commandBuffer.transitionImageLayout(*image,
                                        VK_IMAGE_LAYOUT_UNDEFINED,
                                        VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
                                        VK_PIPELINE_STAGE_TRANSFER_BIT,
                                        VK_PIPELINE_STAGE_TRANSFER_BIT);
    commandBuffer.copyBufferToImage(*stagingBuffer, *image);
    commandBuffer.transitionImageLayout(
        *image,
        VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
        VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
        VK_PIPELINE_STAGE_TRANSFER_BIT,
        VK_PIPELINE_STAGE_ALL_COMMANDS_BIT);

    garbage.push_back(std::move(stagingBuffer));
    mImages.push_back(std::move(image));
  }

  commandBuffer.end();
  vulkan::checkResult(queue.submit(commandBuffer));
  vulkan::checkResult(queue.waitIdle());

  mSampler = makeUnique<vulkan::Sampler>({.device = mDevice});

  // Configure global texture descriptor set
  constexpr size_t MAX_TEXTURES = 1000;

  if (mEnableRayTracing) {
    mDescriptor.pool = makeUnique<vulkan::DescriptorPool>(
        {.device = mDevice,
         .poolSizes = {
             {VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, MAX_TEXTURES},
             {VK_DESCRIPTOR_TYPE_ACCELERATION_STRUCTURE_KHR, 1},
         }});

    mDescriptor.layout = makeUnique<vulkan::DescriptorSetLayout>({
        .device = mDevice,
        .bindings =
            {{
                 .binding = {.binding = 0,
                             .descriptorType =
                                 VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,
                             .descriptorCount = MAX_TEXTURES,
                             .stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT |
                                           VK_SHADER_STAGE_COMPUTE_BIT},
                 .flags = VK_DESCRIPTOR_BINDING_PARTIALLY_BOUND_BIT,
             },
             {
                 .binding = {.binding = 1,
                             .descriptorType =
                                 VK_DESCRIPTOR_TYPE_ACCELERATION_STRUCTURE_KHR,
                             .descriptorCount = 1,
                             .stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT |
                                           VK_SHADER_STAGE_COMPUTE_BIT},
             }},
    });
  } else {
    mDescriptor.pool = makeUnique<vulkan::DescriptorPool>(
        {.device = mDevice,
         .poolSizes = {
             {VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, MAX_TEXTURES},
         }});

    mDescriptor.layout = makeUnique<vulkan::DescriptorSetLayout>({
        .device = mDevice,
        .bindings = {{
            .binding = {.binding = 0,
                        .descriptorType =
                            VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,
                        .descriptorCount = MAX_TEXTURES,
                        .stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT |
                                      VK_SHADER_STAGE_COMPUTE_BIT},
            .flags = VK_DESCRIPTOR_BINDING_PARTIALLY_BOUND_BIT,
        }},
    });
  }

  mDescriptor.set = makeUnique<vulkan::DescriptorSet>({
      .device = mDevice,
      .pool = *mDescriptor.pool,
      .layout = *mDescriptor.layout,
  });

  vulkan::DescriptorSet::Resources resources;
  for (size_t i = 0; i < mImages.size(); ++i) {
    resources.images[0][i] = {
        .sampler = mSampler->vkHandle(),
        .imageView = mImages[i]->getView().vkHandle(),
        .imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL};
  }
  mDescriptor.set->update(resources);

  // Acceleration Structure
  if (mEnableRayTracing) {
    mDevice.submitAndWait([&](const auto& commandBuffer) {
      buildAccelerationStructure(commandBuffer);
    });

    mDescriptor.set->update({.accelerationStructures = {
                                 {1, {{0, mAcceleration.tlas->vkHandle()}}}}});
  }
}

void GPUScene::update(const vulkan::CommandBuffer& commandBuffer,
                      vulkan::RenderFrame& currentFrame) {
  // Update changes

  if (mEnableCameraJitter) {  // Handle camera jitter
    const std::array<glm::vec2, 16> HALTON_SEQUENCE_16 = {
        glm::vec2{0.5f, 0.3333333333333333f},
        glm::vec2{0.25f, 0.6666666666666666f},
        glm::vec2{0.75f, 0.1111111111111111f},
        glm::vec2{0.125f, 0.4444444444444444f},
        glm::vec2{0.625f, 0.7777777777777777f},
        glm::vec2{0.375f, 0.2222222222222222f},
        glm::vec2{0.875f, 0.5555555555555556f},
        glm::vec2{0.0625f, 0.8888888888888888f},
        glm::vec2{0.5625f, 0.037037037037037035f},
        glm::vec2{0.3125f, 0.37037037037037035f},
        glm::vec2{0.8125f, 0.7037037037037037f},
        glm::vec2{0.1875f, 0.14814814814814814f},
        glm::vec2{0.6875f, 0.48148148148148145f},
        glm::vec2{0.4375f, 0.8148148148148147f},
        glm::vec2{0.9375f, 0.25925925925925924f},
        glm::vec2{0.03125f, 0.5925925925925926f},
    };

    const auto& camera = mScene.getCamera();
    auto jitterShear =
        (HALTON_SEQUENCE_16.at(mScene.getFrameCount() % 16) - glm::vec2(0.5f)) *
        (glm::vec2(2.f) / glm::vec2(camera.getWidth(), camera.getHeight()));

    glm::mat4 jitter = glm::mat4(1.f,
                                 0.f,
                                 0.f,
                                 0.f,
                                 0.f,
                                 1.f,
                                 0.f,
                                 0.f,
                                 0.f,
                                 0.f,
                                 1.f,
                                 0.f,
                                 jitterShear.x,
                                 jitterShear.y,
                                 0.f,
                                 1.f);

    static auto prevViewProjection = glm::mat4(1.f);

    mSceneData.camera.prevViewProjection = jitter * prevViewProjection;
    mSceneData.camera.viewProjection = jitter * camera.getViewProjection();

    prevViewProjection = camera.getViewProjection();
  } else {
    mSceneData.camera.prevViewProjection = mSceneData.camera.viewProjection;
    mSceneData.camera.viewProjection = mScene.getCamera().getViewProjection();
  }
  mSceneData.camera.position = mScene.getCamera().getPosition();

  mSceneData.frameCount = mScene.getFrameCount();
  mSceneData.staticFrameCount = mScene.getStaticFrameCount();

  // Upload changes
  {  // Scene data
    auto staging = makeUnique<vulkan::Buffer>({
        .device = mDevice,
        .size = sizeof(mSceneData),
        .usage = VK_BUFFER_USAGE_TRANSFER_SRC_BIT,
        .memoryUsage = VMA_MEMORY_USAGE_CPU_TO_GPU,
    });
    staging->upload(&mSceneData);

    commandBuffer.memoryBarrier(VK_ACCESS_MEMORY_WRITE_BIT,
                                VK_ACCESS_MEMORY_WRITE_BIT,
                                VK_PIPELINE_STAGE_TRANSFER_BIT,
                                VK_PIPELINE_STAGE_TRANSFER_BIT);

    commandBuffer.copyBufferToBuffer(*staging, *mBuffers.sceneData);

    currentFrame.deferDestroy(std::move(staging));
  }

  // TODO: update flag when lights changed
  {
    auto staging = makeUnique<vulkan::Buffer>({
        .device = mDevice,
        .size = sizeof(Light) * mScene.getLights().size(),
        .usage = VK_BUFFER_USAGE_TRANSFER_SRC_BIT,
        .memoryUsage = VMA_MEMORY_USAGE_CPU_TO_GPU,
    });
    staging->upload(mScene.getLights().data());

    commandBuffer.memoryBarrier(VK_ACCESS_MEMORY_WRITE_BIT,
                                VK_ACCESS_MEMORY_WRITE_BIT,
                                VK_PIPELINE_STAGE_TRANSFER_BIT,
                                VK_PIPELINE_STAGE_TRANSFER_BIT);

    commandBuffer.copyBufferToBuffer(*staging, *mBuffers.lights);
    currentFrame.deferDestroy(std::move(staging));
  }

  // if (is_set(mScene.getUpdates(), Scene::UpdateFlags::ModelMatrix))
  {  // Model Matrices
    auto staging = makeUnique<vulkan::Buffer>({
        .device = mDevice,
        .size = sizeof(glm::mat4) * mScene.getModelMatrices().size(),
        .usage = VK_BUFFER_USAGE_TRANSFER_SRC_BIT,
        .memoryUsage = VMA_MEMORY_USAGE_CPU_TO_GPU,
    });
    staging->upload(mScene.getModelMatrices().data());

    commandBuffer.memoryBarrier(VK_ACCESS_MEMORY_WRITE_BIT,
                                VK_ACCESS_MEMORY_WRITE_BIT,
                                VK_PIPELINE_STAGE_TRANSFER_BIT,
                                VK_PIPELINE_STAGE_TRANSFER_BIT);

    commandBuffer.copyBufferToBuffer(*staging, *mBuffers.modelMatrices);
    currentFrame.deferDestroy(std::move(staging));

    const auto& geometryInstances = mScene.getGeometryInstances();
    const auto& modelMatrices = mScene.getModelMatrices();
    for (uint32_t i = 0; i < geometryInstances.size(); i++) {
      const auto& geometryInstance = geometryInstances.at(i);
      const auto& T = modelMatrices[geometryInstance.modelMatrixID];
      VkTransformMatrixKHR transformMatrix = glmToVulkanTransform(T);

      mAcceleration.tlas->updateTransformMatrix(i, transformMatrix);
    }
    mAcceleration.tlas->build(commandBuffer);
  }
}

void GPUScene::createBLASes() {
  VkTransformMatrixKHR transformMatrixIdentity = {
      1.0f, 0.0f, 0.0f, 0.0f, 0.0f, 1.0f, 0.0f, 0.0f, 0.0f, 0.0f, 1.0f, 0.0f};

  mAcceleration.identityTransform = makeUnique<vulkan::Buffer>({
      .device = mDevice,
      .size = sizeof(VkTransformMatrixKHR),
      .usage =
          VK_BUFFER_USAGE_ACCELERATION_STRUCTURE_BUILD_INPUT_READ_ONLY_BIT_KHR |
          VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT,
      .memoryUsage = VMA_MEMORY_USAGE_CPU_TO_GPU,
  });
  mAcceleration.identityTransform->upload(&transformMatrixIdentity);

  const auto& meshes = mScene.getMeshes();
  mAcceleration.blases.reserve(meshes.size());
  for (const auto& mesh : meshes) {
    auto blas = makeUnique<vulkan::BLAS>({
        .device = mDevice,
        .mesh =
            {
                .vertices = *mBuffers.vertices,
                .indices = *mBuffers.indices,
                .transforms = *mAcceleration.identityTransform,
                .firstIndex = mesh.firstIndex,
                .indexCount = mesh.indexCount,
                .vertexSize = sizeof(Vertex),
                .transformOffset = 0,
            },
    });
    mAcceleration.blases.emplace_back(std::move(blas));
  }
}

void GPUScene::createTLAS() {
  const auto& geometryInstances = mScene.getGeometryInstances();
  const auto& modelMatrices = mScene.getModelMatrices();
  const auto& blases = mAcceleration.blases;

  std::vector<vulkan::TLAS::Instance> tlasInstances;
  tlasInstances.reserve(geometryInstances.size());
  for (const auto& geometryInstance : geometryInstances) {
    const auto& T = modelMatrices[geometryInstance.modelMatrixID];
    VkTransformMatrixKHR transformMatrix = glmToVulkanTransform(T);

    tlasInstances.push_back({
        .blas = *blases.at(geometryInstance.meshID),
        .instanceID = 0,
        .transform = transformMatrix,
        .flags = VK_GEOMETRY_INSTANCE_TRIANGLE_FACING_CULL_DISABLE_BIT_KHR,
    });
  }

  mAcceleration.tlas = makeUnique<vulkan::TLAS>({
      .device = mDevice,
      .instances = tlasInstances,
  });
}

void GPUScene::buildAccelerationStructure(
    const vulkan::CommandBuffer& commandBuffer) {
  createBLASes();
  createTLAS();

  for (auto& blas : mAcceleration.blases) {
    blas->build(commandBuffer);
  }

  commandBuffer.memoryBarrier(
      VK_ACCESS_ACCELERATION_STRUCTURE_WRITE_BIT_KHR,
      VK_ACCESS_ACCELERATION_STRUCTURE_READ_BIT_KHR,
      VK_PIPELINE_STAGE_ACCELERATION_STRUCTURE_BUILD_BIT_KHR,
      VK_PIPELINE_STAGE_ACCELERATION_STRUCTURE_BUILD_BIT_KHR);

  mAcceleration.tlas->build(commandBuffer);
}

}  // namespace recore::scene
