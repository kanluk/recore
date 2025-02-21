#pragma once

#include <map>

#include "device.h"

namespace recore::vulkan {

class TimeIntervalQuery;

class TimestampQueryPool : public Object<VkQueryPool> {
 public:
  struct Desc {
    const Device& device;
    uint32_t queryCount = 100;
  };

  explicit TimestampQueryPool(const Desc& desc);

  ~TimestampQueryPool() override;

  void loadResults();

  [[nodiscard]] uint32_t getQueryCount() const { return mQueryCount; }

  [[nodiscard]] const TimeIntervalQuery& requestTimeIntervalQuery(
      const std::string& name);

  void reset() { mActiveQueryCount = 0; }

  [[nodiscard]] const auto& getResults() const { return mResults; }

  [[nodiscard]] const auto& getQueries() const { return mIntervalQueries; }

 private:
  uint32_t mQueryCount{0};
  uint32_t mActiveQueryCount{0};

  std::vector<uint64_t> mResults;
  std::map<std::string, uPtr<TimeIntervalQuery>> mIntervalQueries;
};

class TimeIntervalQuery : public NoCopyMove {
 public:
  TimeIntervalQuery(const TimestampQueryPool& queryPool,
                    uint32_t startQueryIndex)
      : mQueryPool{queryPool}, mStartQueryIndex{startQueryIndex} {}

  ~TimeIntervalQuery() = default;

  void begin(const CommandBuffer& commandBuffer) const;

  void end(const CommandBuffer& commandBuffer) const;

  [[nodiscard]] uint32_t getStartIndex() const { return mStartQueryIndex; }

  [[nodiscard]] uint64_t getStartTimestamp() const;

  [[nodiscard]] float getTime() const;

 private:
  const TimestampQueryPool& mQueryPool;
  uint32_t mStartQueryIndex;
};

class ScopedTimeIntervalQuery : public NoCopyMove {
 public:
  ScopedTimeIntervalQuery(TimestampQueryPool& pool,
                          const CommandBuffer& commandBuffer,
                          const std::string& name)
      : mQuery{pool.requestTimeIntervalQuery(name)},
        mCommandBuffer{commandBuffer},
        mName{name} {
    mQuery.begin(mCommandBuffer);
  }

  ~ScopedTimeIntervalQuery() { mQuery.end(mCommandBuffer); }

 private:
  const TimeIntervalQuery& mQuery;
  const CommandBuffer& mCommandBuffer;
  std::string mName;
};

}  // namespace recore::vulkan

// NOLINTNEXTLINE(cppcoreguidelines-macro-usage)
#define RECORE_GPU_PROFILE_SCOPE(FRAME, COMMAND_BUFFER, NAME) \
  RECORE_DEBUG_SCOPE(COMMAND_BUFFER, NAME)                    \
  recore::vulkan::ScopedTimeIntervalQuery ___gpuProfileScope{ \
      (FRAME).getTimestampQueryPool(), COMMAND_BUFFER, NAME};
