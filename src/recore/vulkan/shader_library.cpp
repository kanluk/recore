#include "shader_library.h"

#include <fstream>
#include <iostream>

#include <libshaderc_util/file_finder.h>
#include <libshaderc_util/io_shaderc.h>
#include <shaderc/shaderc.hpp>

#include <spirv_glsl.hpp>

// NOLINTBEGIN ugly shaderc includer

shaderc_include_result* MakeErrorIncludeResult(const char* message) {
  return new shaderc_include_result{"", 0, message, strlen(message)};
}

// From https://github.com/google/shaderc/tree/main/glslc/
class FileIncluder : public shaderc::CompileOptions::IncluderInterface {
 public:
  explicit FileIncluder(const shaderc_util::FileFinder* file_finder)
      : file_finder_(*file_finder) {}

  ~FileIncluder() override = default;

  // Resolves a requested source file of a given type from a requesting
  // source into a shaderc_include_result whose contents will remain valid
  // until it's released.
  shaderc_include_result* GetInclude(const char* requested_source,
                                     shaderc_include_type include_type,
                                     const char* requesting_source,
                                     size_t include_depth) override {
    const std::string full_path =
        (include_type == shaderc_include_type_relative)
            ? file_finder_.FindRelativeReadableFilepath(requesting_source,
                                                        requested_source)
            : file_finder_.FindReadableFilepath(requested_source);

    if (full_path.empty())
      return MakeErrorIncludeResult("Cannot find or open include file.");

    // In principle, several threads could be resolving includes at the same
    // time.  Protect the included_files.

    // Read the file and save its full path and contents into stable addresses.
    FileInfo* new_file_info = new FileInfo{full_path, {}};
    if (!shaderc_util::ReadFile(full_path, &(new_file_info->contents))) {
      return MakeErrorIncludeResult("Cannot read file");
    }

    included_files_.insert(full_path);

    return new shaderc_include_result{new_file_info->full_path.data(),
                                      new_file_info->full_path.length(),
                                      new_file_info->contents.data(),
                                      new_file_info->contents.size(),
                                      new_file_info};
  }

  // Releases an include result.
  void ReleaseInclude(shaderc_include_result* include_result) override {
    FileInfo* info = static_cast<FileInfo*>(include_result->user_data);
    delete info;
    delete include_result;
  }

  // Returns a reference to the member storing the set of included files.
  const std::unordered_set<std::string>& file_path_trace() const {
    return included_files_;
  }

 private:
  // Used by GetInclude() to get the full filepath.
  const shaderc_util::FileFinder& file_finder_;

  // The full path and content of a source file.
  struct FileInfo {
    const std::string full_path;
    std::vector<char> contents;
  };

  // The set of full paths of included files.
  std::unordered_set<std::string> included_files_;
};

// NOLINTEND

namespace recore::vulkan {

// read spirv binary and output std::vector<uint32_t> with spirv code
static std::vector<uint32_t> readSpirvBinary(
    const std::filesystem::path& path) {
  std::ifstream file{path, std::ios::ate | std::ios::binary};

  if (!file.is_open()) {
    throw std::runtime_error("Failed to open file: " + path.string());
  }

  size_t fileSize = (size_t)file.tellg();
  std::vector<char> buffer(fileSize);
  file.seekg(0);
  file.read(buffer.data(), static_cast<long>(fileSize));
  file.close();

  // convert every 4 chars in the char buffer to uint32_t
  // std::vector<uint32_t> spirvCode(fileSize / sizeof(uint32_t));
  std::vector<uint32_t> spirvCode;
  spirvCode.assign(reinterpret_cast<uint32_t*>(buffer.data()),
                   reinterpret_cast<uint32_t*>(buffer.data()) +
                       static_cast<size_t>(buffer.size() / sizeof(uint32_t)));

  return spirvCode;
}

static std::string readShaderSourceToString(const std::filesystem::path& path) {
  std::ifstream file{path};
  if (!file.is_open()) {
    throw std::runtime_error("Could not open file: " + path.string());
  }
  std::string content{(std::istreambuf_iterator<char>(file)),
                      (std::istreambuf_iterator<char>())};
  file.close();
  return content;
}

static shaderc_shader_kind getShadercKind(VkShaderStageFlags stage) {
  switch (stage) {
    case VK_SHADER_STAGE_VERTEX_BIT:
      return shaderc_vertex_shader;
    case VK_SHADER_STAGE_FRAGMENT_BIT:
      return shaderc_fragment_shader;
    case VK_SHADER_STAGE_COMPUTE_BIT:
      return shaderc_compute_shader;
    default:
      throw std::runtime_error("getShadercKind: invalid type!");
  }
}

VkShaderStageFlagBits getShaderFromExtension(const std::string& filename) {
  std::map<std::string, VkShaderStageFlagBits> extensionDict = {
      {".vert.", VK_SHADER_STAGE_VERTEX_BIT},
      {".frag.", VK_SHADER_STAGE_FRAGMENT_BIT},
      {".comp.", VK_SHADER_STAGE_COMPUTE_BIT},
  };

  for (const auto& [key, value] : extensionDict) {
    if (filename.find(key) != std::string::npos) {
      return value;
    }
  }
  throw std::runtime_error(
      "getShaderFromExtension: could not detect shader stage!");
}

ShaderReflectionData reflectShader(const std::vector<uint32_t>& spirv) {
  ShaderReflectionData reflection;

  spirv_cross::Compiler compiler(spirv);
  // Get workgroup size
  if (compiler.get_execution_model() ==
      spv::ExecutionModel::ExecutionModelGLCompute) {
    auto workgroupSize = compiler.get_execution_mode_argument(
        spv::ExecutionModeLocalSize);
    reflection.workgroupSize = {.x = compiler.get_execution_mode_argument(
                                    spv::ExecutionModeLocalSize, 0),
                                .y = compiler.get_execution_mode_argument(
                                    spv::ExecutionModeLocalSize, 1),
                                .z = compiler.get_execution_mode_argument(
                                    spv::ExecutionModeLocalSize, 2)};
  }

  return reflection;
}

ShaderLibrary::ShaderLibrary(const Device& device,
                             std::filesystem::path rootDir)
    : mDevice{device}, mRootDir{std::move(rootDir)} {}

void ShaderLibrary::loadShaderBinary(const LoadData& loadData) {
  auto stage = loadData.stage.value_or(
      getShaderFromExtension(loadData.path.filename().string()));
  auto spirv = readSpirvBinary(mRootDir / loadData.path);
  auto shader = makeUnique<Shader>(
      {.device = mDevice, .stage = stage, .spriv = spirv});

  ShaderData data{.path = loadData.path, .shader = std::move(shader)};
  mShaders.insert_or_assign(loadData.getName(), std::move(data));
}

class CompilationException : public std::exception {
 public:
  explicit CompilationException(const shaderc::SpvCompilationResult& result)
      : mErrorMessage{result.GetErrorMessage()} {}

  [[nodiscard]] const char* what() const noexcept override {
    return mErrorMessage.c_str();
  }

 private:
  std::string mErrorMessage;
};

const ShaderData& ShaderLibrary::loadShaderSource(const LoadData& loadData,
                                                  bool reload) {
  if (!mShaders.contains(loadData.getName()) || reload) {
    ShaderData shaderData = compileShader(loadData);
    mShaders.insert_or_assign(loadData.getName(), std::move(shaderData));
  }
  return mShaders.at(loadData.getName());
}

const ShaderData& ShaderLibrary::getShaderData(const std::string& name) const {
  return mShaders.at(name);
}

const Shader& ShaderLibrary::getShader(const std::string& name) const {
  return *mShaders.at(name).shader;
}

const ShaderReflectionData& ShaderLibrary::getReflection(
    const std::string& name) const {
  return mShaders.at(name).reflection;
}

void ShaderLibrary::reload() {
  // TODO: only reload those that have actually changed

  for (auto& [name, data] : mShaders) {
    if (data.path.extension() == ".spv") {
      continue;
    }

    auto shader = std::move(data.shader);
    try {
      loadShaderSource({.name = name, .path = data.path}, true);
    } catch (CompilationException& e) {
      data.shader = std::move(shader);
      std::cerr << e.what() << std::endl;
      continue;
    }
  }
}

ShaderData ShaderLibrary::compileShader(const LoadData& loadData) const {
  shaderc::Compiler compiler{};
  shaderc::CompileOptions options{};
  options.SetTargetEnvironment(shaderc_target_env_vulkan,
                               shaderc_env_version_vulkan_1_3);
  // options.SetOptimizationLevel(shaderc_optimization_level_zero);
  options.SetOptimizationLevel(shaderc_optimization_level_performance);

  // TODO: macros, defines

  auto shaderPath = mRootDir / loadData.path;

  shaderc_util::FileFinder fileFinder{};
  fileFinder.search_path().push_back(shaderPath.parent_path().string());
  fileFinder.search_path().push_back(mRootDir.string());
  options.SetIncluder(std::make_unique<FileIncluder>(&fileFinder));

  auto stage = loadData.stage.value_or(
      getShaderFromExtension(loadData.path.filename().string()));
  auto source = readShaderSourceToString(shaderPath);
  auto result = compiler.CompileGlslToSpv(
      source, getShadercKind(stage), loadData.path.string().c_str(), options);

  if (result.GetCompilationStatus() != shaderc_compilation_status_success) {
    throw CompilationException{result};
  }

  std::vector<uint32_t> spirv = {result.cbegin(), result.cend()};
  auto reflection = reflectShader(spirv);

  auto shader = makeUnique<Shader>(
      {.device = mDevice, .stage = stage, .spriv = spirv});

  ShaderData shaderData = {
      .path = loadData.path,
      .shader = std::move(shader),
      .reflection = reflection,
  };
  return shaderData;
}

}  // namespace recore::vulkan
