#pragma once

#include <algorithm>
#include <atomic>
#include <buffer.hpp>
#include <cassert>
#include <concepts>
#include <cstddef>
#include <cstdint>
#include <memory>
#include <new>
#include <optional>
#include <type_traits>
#include <utility>
#include <vector>

namespace async {
inline constexpr std::size_t ALIGNMENT = 2 * sizeof(std::max_align_t);

template <typename T> class Deque {
public:
  explicit Deque(std::int64_t capacity = 1024);

  // Deleting the copy constructor and copy assignment operator
  Deque(Deque const &) = delete;
  Deque &operator=(Deque const &) = delete;

  int64_t capacity() const;
  std::size_t size() const;
  bool empty() const;

  void push(T val);
  std::optional<T> pop() noexcept;
  std::optional<T> steal() noexcept;

  ~Deque() noexcept;

private:
  alignas(ALIGNMENT) std::atomic<std::int64_t> top_;
  alignas(ALIGNMENT) std::atomic<std::int64_t> bottom_;
  alignas(ALIGNMENT) std::atomic<internal::CircularBuffer<T> *> buffer_;

  std::vector<std::unique_ptr<internal::CircularBuffer<T>>> discarded_buffers_;

  static constexpr std::memory_order acquire = std::memory_order_acquire;
  static constexpr std::memory_order consume = std::memory_order_consume;
  static constexpr std::memory_order relaxed = std::memory_order_relaxed;
  static constexpr std::memory_order release = std::memory_order_release;
  static constexpr std::memory_order seq_cst = std::memory_order_seq_cst;
};

template <typename T>
Deque<T>::Deque(std::int64_t capacity)
    : top_(0), bottom_(0), buffer_(new internal::CircularBuffer<T>(capacity)) {
  discarded_buffers_.reserve(32);
}

template <typename T> std::int64_t Deque<T>::capacity() const {
  return buffer_.load(relaxed)->capacity();
}

template <typename T> size_t Deque<T>::size() const {
  std::int64_t top = top_.load(relaxed);
  std::int64_t bottom = bottom_.load(relaxed);
  return static_cast<size_t>(bottom >= top ? (bottom - top) : 0);
}

template <typename T> bool Deque<T>::empty() const { return !size(); }

template <typename T> void Deque<T>::push(T val) {
  std::int64_t bottom = bottom_.load(relaxed);
  std::int64_t top = top_.load(acquire);
  internal::CircularBuffer<T> *buf = buffer_.load(relaxed);

  if (bottom - top > buf->capacity() - 1) {
    discarded_buffers_.emplace_back(
        std::exchange(buf, buf->expandAndCopy(top, bottom)));
    buffer_.store(buf, relaxed);
  }

  buf->set(bottom, std::move(val));
  std::atomic_thread_fence(release);

  bottom_.store(bottom + 1, relaxed);
}

template <typename T> std::optional<T> Deque<T>::pop() noexcept {
  std::int64_t bottom = bottom_.load(relaxed) - 1;
  internal::CircularBuffer<T> *buf = buffer_.load(relaxed);
  bottom_.store(bottom, relaxed);

  std::atomic_thread_fence(seq_cst);

  std::int64_t top = top_.load(relaxed);
  bool failed_to_retrieve = false;

  if (top <= bottom) {
    /* Non-empty queue */
    if (top == bottom) {
      /* Single element in the queue */
      failed_to_retrieve =
          !top_.compare_exchange_strong(top, top + 1, seq_cst, relaxed);
      /* Mark it when you fail the race */
      bottom_.store(bottom + 1, relaxed);
    }
    return failed_to_retrieve ? std::nullopt : buf->get(bottom);
  } else { /* Empty queue */
    bottom_.store(bottom + 1, relaxed);
    return std::nullopt;
  }
}

template <typename T> std::optional<T> Deque<T>::steal() noexcept {
  std::int64_t top = top_.load(acquire);
  std::atomic_thread_fence(seq_cst);
  std::int64_t bottom = bottom_.load(acquire);

  if (top < bottom) {
    /* Non-empty queue */
    internal::CircularBuffer<T> *buf = buffer_.load(consume);
    T x = buf->get(top);

    bool failed_to_retrieve =
        (!top_.compare_exchange_strong(top, top + 1, seq_cst, relaxed));

    return failed_to_retrieve ? std::nullopt : x;
  } else { /* Empty queue */
    return std::nullopt;
  }
}

} // namespace async