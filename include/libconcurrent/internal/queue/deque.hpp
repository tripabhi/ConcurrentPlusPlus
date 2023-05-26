#ifndef __DEQUE_HPP__
#define __DEQUE_HPP__

#include <buffer.hpp>
#include <optional>

namespace async {
template <typename T> class Deque {
  void push(T &lval);
  void push(T &&rval);
  std::optional<T> pop() noexcept;
  std::optional<T> steal() noexcept;
};
} // namespace async

#endif