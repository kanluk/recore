#pragma once

#include "device.h"

namespace recore::vulkan {

class Fence : public Object<VkFence> {
 public:
  struct Desc {
    const Device& device;
  };

  explicit Fence(const Desc& desc);
  ~Fence() override;

  void wait();
  void reset();
};

class FencePool {
 public:
  explicit FencePool(const Device& device);
  ~FencePool();

  FencePool(const FencePool&) = delete;
  FencePool& operator=(const FencePool&) = delete;
  FencePool(FencePool&&) = delete;
  FencePool& operator=(FencePool&&) = delete;

  [[nodiscard]] Fence& request();
  void wait();
  void reset();

 private:
  const Device& mDevice;
  std::vector<uPtr<Fence>> mFences;
  uint32_t mActiveCount{0};
};

class Semaphore : public Object<VkSemaphore> {
 public:
  struct Desc {
    const Device& device;
  };

  explicit Semaphore(const Desc& desc);
  ~Semaphore() override;
};

class SemaphorePool {
 public:
  explicit SemaphorePool(const Device& device);
  ~SemaphorePool();

  SemaphorePool(const SemaphorePool&) = delete;
  SemaphorePool& operator=(const SemaphorePool&) = delete;
  SemaphorePool(SemaphorePool&&) = delete;
  SemaphorePool& operator=(SemaphorePool&&) = delete;

  [[nodiscard]] Semaphore& request();
  [[nodiscard]] uPtr<Semaphore> requestWithOwnership();

  void releaseOwned(uPtr<Semaphore>&& semaphore);
  void reset();

 private:
  const Device& mDevice;

  std::vector<uPtr<Semaphore>> mSemaphores;
  std::vector<uPtr<Semaphore>> mReleasedSemaphores;
  uint32_t mActiveCount{0};
};

}  // namespace recore::vulkan
