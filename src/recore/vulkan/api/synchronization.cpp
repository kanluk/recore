#include "synchronization.h"

namespace recore::vulkan {

Fence::Fence(const Desc& desc) : Object{desc.device} {
  VkFenceCreateInfo fenceInfo{};
  fenceInfo.sType = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO;
  fenceInfo.flags = 0;

  checkResult(vkCreateFence(mDevice.vkHandle(), &fenceInfo, nullptr, &mHandle));
}

Fence::~Fence() {
  vkDestroyFence(mDevice.vkHandle(), mHandle, nullptr);
}

void Fence::wait() {
  vkWaitForFences(mDevice.vkHandle(), 1, &mHandle, VK_TRUE,
                  std::numeric_limits<uint64_t>::max());
}

void Fence::reset() {
  vkResetFences(mDevice.vkHandle(), 1, &mHandle);
}

FencePool::FencePool(const Device& device) : mDevice{device} {}

FencePool::~FencePool() {
  wait();
  reset();
  mFences.clear();
}

Fence& FencePool::request() {
  if (mActiveCount < mFences.size()) {
    return *mFences.at(mActiveCount++);
  }

  mFences.push_back(makeUnique<Fence>({.device = mDevice}));
  mActiveCount++;
  return *mFences.back();
}

void FencePool::wait() {
  // TODO: more efficient implementation with only one vkWaitForFences call
  for (auto& fence : mFences) {
    fence->wait();
  }
}

void FencePool::reset() {
  // TODO: more efficient implementation with only one vkResetFences call
  for (auto& fence : mFences) {
    fence->reset();
  }

  mActiveCount = 0;
}

Semaphore::Semaphore(const Desc& desc) : Object{desc.device} {
  VkSemaphoreCreateInfo semaphoreInfo{};
  semaphoreInfo.sType = VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO;
  semaphoreInfo.flags = 0;

  checkResult(
      vkCreateSemaphore(mDevice.vkHandle(), &semaphoreInfo, nullptr, &mHandle));
}

Semaphore::~Semaphore() {
  vkDestroySemaphore(mDevice.vkHandle(), mHandle, nullptr);
}

SemaphorePool::SemaphorePool(const Device& device) : mDevice{device} {}

SemaphorePool::~SemaphorePool() {
  reset();
  mSemaphores.clear();
}

Semaphore& SemaphorePool::request() {
  if (mActiveCount < mSemaphores.size()) {
    return *mSemaphores.at(mActiveCount++);
  }

  mSemaphores.push_back(makeUnique<Semaphore>({.device = mDevice}));
  mActiveCount++;
  return *mSemaphores.back();
}

uPtr<Semaphore> SemaphorePool::requestWithOwnership() {
  if (mActiveCount < mSemaphores.size()) {
    auto semaphore = std::move(mSemaphores.back());
    mSemaphores.pop_back();
    return semaphore;
  }

  return makeUnique<Semaphore>({.device = mDevice});
}

void SemaphorePool::releaseOwned(uPtr<Semaphore>&& semaphore) {
  mReleasedSemaphores.push_back(std::move(semaphore));
}

void SemaphorePool::reset() {
  mActiveCount = 0;

  for (auto& semaphore : mReleasedSemaphores) {
    mSemaphores.push_back(std::move(semaphore));
  }

  mReleasedSemaphores.clear();
}

}  // namespace recore::vulkan
