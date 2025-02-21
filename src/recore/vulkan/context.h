#pragma once

#include <recore/vulkan/api/command.h>
#include <recore/vulkan/api/device.h>
#include <recore/vulkan/api/image.h>
#include <recore/vulkan/api/instance.h>
#include <recore/vulkan/api/queries.h>
#include <recore/vulkan/api/renderpass.h>
#include <recore/vulkan/api/swapchain.h>
#include <recore/vulkan/api/synchronization.h>

namespace recore::vulkan {

class RenderFrame : public NoCopyMove {
 public:
  explicit RenderFrame(const Device& device);
  ~RenderFrame() = default;

  void reset();

  [[nodiscard]] Semaphore& requestSemaphore();
  [[nodiscard]] uPtr<Semaphore> requestSemaphoreWithOwnership();
  void releaseOwnedSemaphore(uPtr<Semaphore> semaphore);

  [[nodiscard]] Fence& requestFence();

  [[nodiscard]] CommandBuffer& getCommandBuffer() { return mCommandBuffer; }

  [[nodiscard]] TimestampQueryPool& getTimestampQueryPool() {
    return mTimestampQueryPool;
  }

  void deferDestroy(uPtr<Buffer>&& buffer) {
    mGarbage.buffers.push_back(std::move(buffer));
  }

 private:
  const Device& mDevice;

  CommandPool mCommandPool;
  CommandBuffer mCommandBuffer;

  SemaphorePool mSemaphorePool;
  FencePool mFencePool;

  TimestampQueryPool mTimestampQueryPool;

  struct {
    std::vector<uPtr<Buffer>> buffers;
  } mGarbage;
};

class RenderContext : public NoCopyMove {
 public:
  struct Desc {
    const Device& device;
    const Surface& surface;
    uint32_t width;
    uint32_t height;
    uint32_t numFramesInFlight = 2;
  };

  using FrameRecorder = std::function<void(const CommandBuffer& commandBuffer)>;

  explicit RenderContext(const Desc& desc);
  ~RenderContext() = default;

  [[nodiscard]] const Device& getDevice() const { return mDevice; }

  [[nodiscard]] RenderFrame& getCurrentFrame() {
    return *mRenderFrames[mCurrentFrameIndex];
  }

  [[nodiscard]] uint32_t getNumFramesInFlight() const {
    return mNumFramesInFlight;
  }

  [[nodiscard]] RenderFrame& beginFrame();

  void submit(FrameRecorder&& recorder);

  void endFrame(const Image& finalImage);

  void resize(uint32_t width, uint32_t height);

 private:
  bool checkSurfaceUpdate();
  void recreateSwapchain();

  const Device& mDevice;
  const Surface& mSurface;

  uPtr<Swapchain> mSwapchain;
  VkExtent2D mSurfaceExtent{};

  std::vector<uPtr<RenderFrame>> mRenderFrames;
  uint32_t mNumFramesInFlight{1};
  uint32_t mCurrentFrameIndex{0};

  std::vector<FrameRecorder> mFrameRecordBuffer;
};

class HeadlessContext : public NoCopyMove {
 public:
  struct Desc {
    const Device& device;
    uint32_t width;
    uint32_t height;
    uint32_t numFramesInFlight = 2;
  };

  using FrameRecorder = std::function<void(const CommandBuffer& commandBuffer)>;

  explicit HeadlessContext(const Desc& desc);
  ~HeadlessContext() = default;

  [[nodiscard]] const Device& getDevice() const { return mDevice; }

  [[nodiscard]] RenderFrame& getCurrentFrame() {
    return *mRenderFrames[mCurrentFrameIndex];
  }

  [[nodiscard]] uint32_t getNumFramesInFlight() const {
    return mNumFramesInFlight;
  }

  [[nodiscard]] RenderFrame& beginFrame();

  void submit(FrameRecorder&& recorder);

  void endFrame(const Image& finalImage);

 private:
  const Device& mDevice;

  std::vector<uPtr<RenderFrame>> mRenderFrames;
  uint32_t mNumFramesInFlight{1};
  uint32_t mCurrentFrameIndex{0};

  std::vector<FrameRecorder> mFrameRecordBuffer;
};

}  // namespace recore::vulkan
