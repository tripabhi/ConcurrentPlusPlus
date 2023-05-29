#pragma once

#include <atomic>
#include <cassert>
#include <cstdint>
#include <memory>
#include <optional>
#include <type_traits>
#include <utility>
#include <vector>

#include <async/internal/buffer.h>

namespace async {

template <typename T> class Deque {
public:
  explicit Deque(std::int64_t cap = 1024);

  Deque(Deque const &other) = delete;
  Deque &operator=(Deque const &other) = delete;

  std::size_t size() const noexcept;
  int64_t capacity() const noexcept;
  bool empty() const noexcept;

  template <typename... Args> void push(Args &&... args);

  std::optional<T> pop() noexcept(no_alloc ||
                                  std::is_nothrow_move_constructible_v<T>);

  std::optional<T> steal() noexcept(no_alloc ||
                                    std::is_nothrow_move_constructible_v<T>);

  ~Deque();

private:
  static constexpr bool no_alloc = std::conjunction_v<
      std::is_trivially_copyable<T>, std::is_copy_constructible<T>,
      std::is_move_constructible<T>, std::is_copy_assignable<T>,
      std::is_move_assignable<T>, std::is_trivially_destructible<T>>;

  using buffer_t =
      internal::CircularBuffer<std::conditional_t<no_alloc, T, T *>>;

  std::atomic<std::int64_t> top_;
  std::atomic<std::int64_t> bottom_;
  std::atomic<buffer_t *> buffer_;

  std::vector<std::unique_ptr<buffer_t>> discarded_buffers_;

  static constexpr std::memory_order acquire = std::memory_order_acquire;
  static constexpr std::memory_order consume = std::memory_order_consume;
  static constexpr std::memory_order relaxed = std::memory_order_relaxed;
  static constexpr std::memory_order release = std::memory_order_release;
  static constexpr std::memory_order seq_cst = std::memory_order_seq_cst;
};

template <typename T>
Deque<T>::Deque(std::int64_t capacity)
    : top_(0), bottom_(0), buffer_(new buffer_t{capacity}) {
  discarded_buffers_.reserve(32);
}

template <typename T> std::size_t Deque<T>::size() const noexcept {
  int64_t b = bottom_.load(relaxed);
  int64_t t = top_.load(relaxed);
  return static_cast<std::size_t>(b >= t ? b - t : 0);
}

template <typename T> int64_t Deque<T>::capacity() const noexcept {
  return buffer_.load(relaxed)->capacity();
}

template <typename T> bool Deque<T>::empty() const noexcept { return !size(); }

template <typename T>
template <typename... Args>
void Deque<T>::push(Args &&... args) {

  std::int64_t bottom = bottom_.load(relaxed);
  std::int64_t top = top_.load(acquire);
  auto *buf = buffer_.load(relaxed);

  if (bottom - top > buf->capacity() - 1) {
    discarded_buffers_.emplace_back(
        std::exchange(buf, buf->expandAndCopy(top, bottom)));
    buffer_.store(buf, relaxed);
  }

  if constexpr (no_alloc) {
    buf->set(bottom, {std::forward<Args>(args)...});
  } else {
    buf->set(bottom, new T{std::forward<Args>(args)...});
  }

  std::atomic_thread_fence(release);
  bottom_.store(bottom + 1, relaxed);
}

template <typename T>
std::optional<T>
Deque<T>::pop() noexcept(no_alloc || std::is_nothrow_move_constructible_v<T>) {

  std::int64_t new_bottom = bottom_.load(relaxed) - 1;
  auto *buf = buffer_.load(relaxed);

  bottom_.store(new_bottom, relaxed);

  std::atomic_thread_fence(seq_cst);

  std::int64_t top = top_.load(relaxed);

  if (top <= new_bottom) {
    if (top == new_bottom) {
      if (!top_.compare_exchange_strong(top, top + 1, seq_cst, relaxed)) {
        bottom_.store(new_bottom + 1, relaxed);
        return std::nullopt;
      }
      bottom_.store(new_bottom + 1, relaxed);
    }

    auto x = buf->get(new_bottom);

    if constexpr (no_alloc) {
      return x;
    } else {
      std::optional tmp{std::move(*x)};
      delete x;
      return tmp;
    }

  } else {
    bottom_.store(new_bottom + 1, relaxed);
    return std::nullopt;
  }
}

template <typename T>
std::optional<T>
Deque<T>::steal() noexcept(no_alloc ||
                           std::is_nothrow_move_constructible_v<T>) {
  std::int64_t top = top_.load(acquire);
  std::atomic_thread_fence(seq_cst);
  std::int64_t bottom = bottom_.load(acquire);

  if (top < bottom) {
    auto x = buffer_.load(consume)->get(top);

    if (!top_.compare_exchange_strong(top, top + 1, seq_cst, relaxed)) {
      return std::nullopt;
    }

    if constexpr (no_alloc) {
      return x;
    } else {
      std::optional tmp{std::move(*x)};
      delete x;
      return tmp;
    }

  } else {
    return std::nullopt;
  }
}

template <typename T> Deque<T>::~Deque() {
  if constexpr (!no_alloc) {
    while (!empty()) {
      pop();
    }
  }
  delete buffer_.load();
}

} // namespace async