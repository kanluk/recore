#pragma once

#include <recore/core/base.h>
#include <string>

#include <vulkan/vulkan.h>

namespace recore::vulkan {

class DebugMessenger : public NoCopyMove {
 public:
  struct Report {
    VkDebugUtilsMessageSeverityFlagBitsEXT messageSeverity;
    std::string message;
  };

  struct ShaderPrint {
    std::string message;
  };

  DebugMessenger() = default;

  void report(const Report& report) {
    if (report.messageSeverity ==
            VK_DEBUG_UTILS_MESSAGE_SEVERITY_INFO_BIT_EXT &&
        report.message.contains("VVL-DEBUG-PRINTF")) {
      constexpr const std::string_view BEFORE_PRINT = "|";
      auto pos = report.message.find_last_of(BEFORE_PRINT);
      if (pos != std::string::npos) {
        mShaderPrints.push_back(report.message.substr(pos + 2));
      }
    }
    mReports.push_back(report);
  }

  [[nodiscard]] const std::vector<Report>& getReports() const {
    return mReports;
  }

  [[nodiscard]] const std::vector<std::string>& getShaderPrints() const {
    return mShaderPrints;
  }

  void startNextFrame() {
    mShaderPrints.clear();
    mReports.clear();
  }

 private:
  std::vector<Report> mReports;
  std::vector<std::string> mShaderPrints;
};

}  // namespace recore::vulkan
