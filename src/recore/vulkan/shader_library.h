#pragma once

#include <filesystem>
#include <optional>
#include <unordered_map>

#include <recore/vulkan/api/pipeline.h>

namespace recore::vulkan {

struct ShaderReflectionData {
  struct WorkgroupSize {
    uint32_t x;
    uint32_t y;
    uint32_t z;
  };

  std::optional<WorkgroupSize> workgroupSize;  // only for compute
};

struct ShaderData {
  std::filesystem::path path;
  uPtr<Shader> shader;
  ShaderReflectionData reflection;

  // NOLINTNEXTLINE(hicpp-explicit-conversions)
  operator Shader*() const { return &*shader; }
};

class ShaderLibrary {
 public:
  struct LoadData {
    std::optional<std::string> name;
    std::filesystem::path path;
    std::optional<VkShaderStageFlagBits> stage;

    [[nodiscard]] std::string getName() const {
      if (name.has_value()) {
        return name.value();
      }
      return path.filename().string();
    }
  };

  ShaderLibrary(const Device& device, std::filesystem::path rootDir);
  ~ShaderLibrary() = default;

  ShaderLibrary(const ShaderLibrary&) = delete;
  ShaderLibrary& operator=(const ShaderLibrary&) = delete;
  ShaderLibrary(ShaderLibrary&&) = delete;
  ShaderLibrary& operator=(ShaderLibrary&&) = delete;

  void loadShaderBinary(const LoadData& loadData);
  const ShaderData& loadShaderSource(const LoadData& loadData,
                                     bool reload = false);

  const ShaderData& loadShader(const std::filesystem::path& path) {
    return loadShaderSource({.path = path});
  }

  void reload();

  [[nodiscard]] const ShaderData& getShaderData(const std::string& name) const;
  [[nodiscard]] const Shader& getShader(const std::string& name) const;
  [[nodiscard]] const ShaderReflectionData& getReflection(
      const std::string& name) const;

 private:
  [[nodiscard]] ShaderData compileShader(const LoadData& loadData) const;

  const Device& mDevice;
  std::filesystem::path mRootDir;

  std::unordered_map<std::string, ShaderData> mShaders;
};

}  // namespace recore::vulkan
