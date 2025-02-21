#include "queries.h"

#include "command.h"

namespace recore::vulkan {

TimestampQueryPool::TimestampQueryPool(const Desc& desc)
    : Object{desc.device}, mQueryCount{desc.queryCount} {
  VkQueryPoolCreateInfo poolInfo{};
  poolInfo.sType = VK_STRUCTURE_TYPE_QUERY_POOL_CREATE_INFO;
  poolInfo.queryType = VK_QUERY_TYPE_TIMESTAMP;
  poolInfo.queryCount = mQueryCount;
  vkCreateQueryPool(mDevice.vkHandle(), &poolInfo, nullptr, &mHandle);

  mResults.resize(mQueryCount);
}

TimestampQueryPool::~TimestampQueryPool() {
  vkDestroyQueryPool(mDevice.vkHandle(), mHandle, nullptr);
}

void TimestampQueryPool::loadResults() {
  if (mActiveQueryCount == 0) {
    return;
  }

  vkGetQueryPoolResults(mDevice.vkHandle(),
                        mHandle,
                        0,
                        mActiveQueryCount,
                        mActiveQueryCount * sizeof(uint64_t),
                        mResults.data(),
                        sizeof(uint64_t),
                        VK_QUERY_RESULT_64_BIT);
}

const TimeIntervalQuery& TimestampQueryPool::requestTimeIntervalQuery(
    const std::string& name) {
  if (!mIntervalQueries.contains(name)) {
    auto query = makeUnique<TimeIntervalQuery>(*this, mActiveQueryCount);
    mIntervalQueries.insert_or_assign(name, std::move(query));
    mActiveQueryCount += 2;
  }
  return *mIntervalQueries.at(name);
}

void TimeIntervalQuery::begin(const CommandBuffer& commandBuffer) const {
  commandBuffer.writeTimestamp(
      mQueryPool, mStartQueryIndex, VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT);
}

void TimeIntervalQuery::end(const CommandBuffer& commandBuffer) const {
  commandBuffer.writeTimestamp(
      mQueryPool, mStartQueryIndex + 1, VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT);
}

uint64_t TimeIntervalQuery::getStartTimestamp() const {
  return mQueryPool.getResults().at(mStartQueryIndex);
}

float TimeIntervalQuery::getTime() const {
  auto timestampPeriod = mQueryPool.getDevice()
                             .getPhysicalDevice()
                             .getProperties()
                             .limits.timestampPeriod;
  const auto& results = mQueryPool.getResults();
  return static_cast<float>(results[mStartQueryIndex + 1] -
                            results[mStartQueryIndex]) *
         timestampPeriod * 1e-6f;
}
}  // namespace recore::vulkan
