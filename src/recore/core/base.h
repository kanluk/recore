#pragma once

#include <memory>

#include <algorithm>
#include <functional>
#include <iterator>
#include <vector>

namespace recore {

// Custom smart pointers

template <typename T>
using uPtr = std::unique_ptr<T>;

template <typename T, typename... Args>
constexpr uPtr<T> makeUnique(Args&&... args) {
  return std::make_unique<T>(std::forward<Args>(args)...);
}

template <typename T>
constexpr uPtr<T> makeUnique(typename T::Desc&& desc) {
  return std::make_unique<T>(std::move(desc));
}

template <typename T>
using sPtr = std::shared_ptr<T>;

template <typename T, typename... Args>
constexpr sPtr<T> makeShared(Args&&... args) {
  return std::make_shared<T>(std::forward<Args>(args)...);
}

template <typename T>
constexpr sPtr<T> makeShared(typename T::Desc&& desc) {
  return std::make_shared<T>(std::move(desc));
}

namespace rstd {

template <typename Input, typename Output>
std::vector<Output> transform(const std::vector<Input>& input,
                              std::function<Output(const Input&)>&& func) {
  std::vector<Output> output;
  std::transform(input.begin(),
                 input.end(),
                 std::back_inserter(output),
                 std::forward<std::function<Output(const Input&)>>(func));
  return output;
}

}  // namespace rstd

class NoCopyMove {
 public:
  NoCopyMove() = default;
  NoCopyMove(const NoCopyMove&) = delete;
  NoCopyMove& operator=(const NoCopyMove&) = delete;
  NoCopyMove(NoCopyMove&&) = delete;
  NoCopyMove& operator=(NoCopyMove&&) = delete;
};

}  // namespace recore

// Stolen from falcor
#define RECORE_ENUM_CLASS_OPERATORS(e_)                                \
  inline e_ operator&(e_ a, e_ b) {                                    \
    return static_cast<e_>(static_cast<int>(a) & static_cast<int>(b)); \
  }                                                                    \
  inline e_ operator|(e_ a, e_ b) {                                    \
    return static_cast<e_>(static_cast<int>(a) | static_cast<int>(b)); \
  }                                                                    \
  inline e_& operator|=(e_& a, e_ b) {                                 \
    a = a | b;                                                         \
    return a;                                                          \
  };                                                                   \
  inline e_& operator&=(e_& a, e_ b) {                                 \
    a = a & b;                                                         \
    return a;                                                          \
  };                                                                   \
  inline e_ operator~(e_ a) {                                          \
    return static_cast<e_>(~static_cast<int>(a));                      \
  }                                                                    \
  inline bool is_set(e_ val, e_ flag) {                                \
    return (val & flag) != static_cast<e_>(0);                         \
  }                                                                    \
  inline void flip_bit(e_& val, e_ flag) {                             \
    val = is_set(val, flag) ? (val & (~flag)) : (val | flag);          \
  }
