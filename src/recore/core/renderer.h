#pragma once

#include <recore/core/base.h>

#include <recore/vulkan/context.h>

namespace recore::core {

class Renderer : public NoCopyMove {
 public:
  virtual ~Renderer() = default;

  virtual void update(float deltaTime) = 0;

  virtual void render(const vulkan::CommandBuffer& commandBuffer,
                      vulkan::RenderFrame& frame) = 0;

  virtual void resize(uint32_t width, uint32_t height) = 0;

  [[nodiscard]] virtual const vulkan::Image& getResultImage() const = 0;
};

}  // namespace recore::core
