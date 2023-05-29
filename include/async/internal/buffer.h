#pragma once

#include <atomic>
#include <cassert>
#include <concepts>
#include <cstdint>
#include <memory>
namespace async {
namespace internal {

/**
 * @class CircularBuffer
 * @brief Represents a circular array buffer
 * @tparam T type of the elements stored in the buffer
 */
template <typename T> class CircularBuffer {
public:
  /**
   * @brief Constructs a CircularBuffer object with a given capacity.
   * @param capacity The capacity of the buffer.
   * @note The capacity must be a power of 2.
   */
  explicit CircularBuffer(std::int64_t capacity)
      : capacity_(capacity), mask_(capacity - 1) {
    assert(capacity && (!(capacity & (capacity - 1))) &&
           "Capacity must be power of 2");
  }

  /**
   * @brief Retrieves the capacity of the buffer.
   * @return The capacity of the buffer.
   */
  std::int64_t capacity() const noexcept { return capacity_; }

  /**
   * @brief Sets the value at a given index in the buffer.
   * @param index The index at which to set the value.
   * @param val The value to set.
   * @note This operation is thread-safe.
   */
  void set(std::int64_t index, T val) noexcept {
    buffer_[index & mask_].store(val, std::memory_order_relaxed);
  }

  /**
   * @brief Retrieves the value at a given index in the buffer.
   * @param index The index from which to retrieve the value.
   * @return The value at the specified index.
   * @note This operation is thread-safe.
   */
  T get(std::int64_t index) const noexcept {
    return buffer_[index & mask_].load(std::memory_order_relaxed);
  }

  /**
   * @brief Expands the buffer and copies the elements from a given range.
   * @param start_inclusive The start index (inclusive) of the range to copy.
   * @param end_exclusive The end index (exclusive) of the range to copy.
   * @return A pointer to the expanded and copied CircularBuffer.
   * @note The caller is responsible for deleting the returned object.
   */
  CircularBuffer<T> *expandAndCopy(std::int64_t start_inclusive,
                                   std::int64_t end_exclusive) {
    CircularBuffer<T> *buf = new CircularBuffer{capacity_ << 1};
    for (std::int64_t i = start_inclusive; i != end_exclusive; i++) {
      buf->set(i, get(i));
    }
    return buf;
  }

private:
  std::int64_t capacity_; /* The capacity of the buffer. */
  std::int64_t mask_;     /* The mask used for indexing into the buffer. */
  std::unique_ptr<std::atomic<T>[]> buffer_ =
      std::make_unique<std::atomic<T>[]>(capacity_); /* The underlying buffer */
};
} // namespace internal
} // namespace async