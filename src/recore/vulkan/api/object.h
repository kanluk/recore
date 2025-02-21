#pragma once

namespace recore::vulkan {

class Device;

template <typename Handle>
class Object {
 public:
  explicit Object(const Device& device) : mDevice{device} {}

  virtual ~Object() = default;

  // Default: delete all copy & move constructors. If needed the subclass has to
  // provide a sensible implementation!
  Object(const Object&) = delete;
  Object& operator=(const Object&) = delete;
  Object(Object&&) = delete;
  Object& operator=(Object&&) = delete;

  [[nodiscard]] Handle vkHandle() const { return mHandle; }

  [[nodiscard]] const Handle* vkPtr() const { return &mHandle; }

  [[nodiscard]] const Device& getDevice() const { return mDevice; }

 protected:
  const Device& mDevice;
  Handle mHandle;
};

}  // namespace recore::vulkan
