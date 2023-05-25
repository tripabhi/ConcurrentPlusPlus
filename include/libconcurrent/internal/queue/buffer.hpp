#ifndef __DEQUE_HPP__
#define __DEQUE_HPP__

#include <cassert>
#include <concepts>
#include <cstdint>
#include <memory>
#include <type_traits>

namespace async {

namespace internal {
template <typename T> class CircularBuffer {
public:
  explicit CircularBuffer(std::int64_t capacity)
      : capacity_(capacity), mask_(capacity - 1) {
    assert(capacity && (!(capacity & (capacity - 1))) &&
           "Capacity must be power of 2");
  }

  std::int64_t capacity() const { return capacity_; }

  void set(std::int64_t index,
           T &&val) requires std::is_nothrow_move_assignable_v<T> {
    array_[index & mask_] = std::move(val);
  }

  T get(std::int64_t index) const
      requires std::is_nothrow_move_constructible_v<T> {
    return array_[index & mask_];
  }

  CircularBuffer *expandAndCopy(std::int64_t start_inclusive,
                                std::int64_t end_exclusive) const {
    CircularBuffer *newBuf = new CircularBuffer{capacity_ << 1};
    for (std::int64_t i = start_inclusive; i < end_exclusive; i++) {
      newBuf->set(i, get(i));
    }
    return newBuf;
  }

private:
  std::int64_t capacity_;
  std::int64_t mask_;

  std::unique_ptr<T[]> array_ = std::make_unique<T[]>(capacity_);
};
} // namespace internal
} // namespace async

#endif