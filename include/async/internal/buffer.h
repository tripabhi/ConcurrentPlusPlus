#pragma once

#include <atomic>
#include <cassert>
#include <concepts>
#include <cstdint>
#include <memory>
namespace async {
namespace internal {
template <typename T> class CircularBuffer {
public:
  explicit CircularBuffer(std::int64_t capacity)
      : capacity_(capacity), mask_(capacity - 1) {
    assert(capacity && (!(capacity & (capacity - 1))) &&
           "Capacity must be power of 2");
  }

  std::int64_t capacity() const noexcept { return capacity_; }

  void set(std::int64_t index, T t) noexcept {
    buffer_[index & mask_].store(t, std::memory_order_relaxed);
  }

  T get(std::int64_t index) const noexcept {
    return buffer_[index & mask_].load(std::memory_order_relaxed);
  }

  CircularBuffer<T> *expandAndCopy(std::int64_t start_inclusive,
                                   std::int64_t end_exclusive) {
    CircularBuffer<T> *buf = new CircularBuffer{capacity_ << 1};
    for (std::int64_t i = start_inclusive; i != end_exclusive; i++) {
      buf->set(i, get(i));
    }
    return buf;
  }

private:
  std::int64_t capacity_;
  std::int64_t mask_;

#if !__cpp_lib_smart_ptr_for_overwrite
  std::unique_ptr<std::atomic<T>[]> buffer_ =
      std::make_unique<std::atomic<T>[]>(capacity_);
#else
  std::unique_ptr<std::atomic<T>[]> buffer_ =
      std::make_unique_for_overwrite<std::atomic<T>[]>(capacity_);
#endif
};
} // namespace internal
} // namespace async