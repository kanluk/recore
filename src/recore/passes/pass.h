#pragma once

#include <recore/core/base.h>

#include <recore/vulkan/api/buffer.h>
#include <recore/vulkan/api/command.h>
#include <recore/vulkan/api/device.h>
#include <recore/vulkan/api/image.h>
#include <recore/vulkan/api/pipeline.h>
#include <recore/vulkan/api/renderpass.h>

#include <recore/vulkan/debug.h>
#include <recore/vulkan/shader_library.h>

#include <recore/vulkan/context.h>

namespace recore::passes {

class Pass : public NoCopyMove {
 public:
  explicit Pass(const vulkan::Device& device) : mDevice{device} {}

  virtual ~Pass() = default;

  virtual void resize(uint32_t width, uint32_t height) {}

  virtual void reloadShaders(vulkan::ShaderLibrary& shaderLibrary) {}

  virtual void execute(const vulkan::CommandBuffer& commandBuffer,
                       vulkan::RenderFrame& frame) = 0;

 protected:
  const vulkan::Device& mDevice;
};

}  // namespace recore::passes
