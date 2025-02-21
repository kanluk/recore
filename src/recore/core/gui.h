#pragma once

#include <recore/core/base.h>

#include <recore/vulkan/context.h>

namespace recore::core {

class GUI : public NoCopyMove {
 public:
  virtual ~GUI() = default;

  virtual void update() = 0;

  virtual void render(const vulkan::CommandBuffer& commandBuffer,
                      vulkan::RenderFrame& frame) = 0;

  virtual void setFramebuffer(const vulkan::Image& image) = 0;
};

}  // namespace recore::core
