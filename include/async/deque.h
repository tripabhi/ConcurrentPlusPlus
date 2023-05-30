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

/**
 * @brief A lock-free and thread-safe double-ended queue (deque) implementation
 * for multi-threaded environments.
 *
 * The Deque class provides a container that allows efficient insertion and
 * removal of elements at both ends. It is designed to be used in multi-threaded
 * environments, offering lock-free operations for improved performance and
 * thread safety guarantees. Owner threads can push() and pop() from the deque
 * effectively converting to a stack, whereas non-owner threads can only use
 * steal().
 *
 * Exception Guarantee:
 * The Deque provides a strong exception guarantee for types that satisfy the
 * following requirements:
 * - The type T is copyable, movable, and destructible.
 * - The type T is nothrow move constructible.
 * - The type T is nothrow move assignable.
 * - The type T is nothrow destructible.
 *
 * For types that meet these requirements, the Deque ensures that operations
 * such as pop() and steal() will not throw exceptions, providing a strong
 * exception safety guarantee. However, for non-trivial types that involve
 * dynamic memory allocation, additional memory management may be required to
 * handle exceptions properly.
 *
 * The algorithm is directly inspired from @link
 * https://dl.acm.org/doi/10.1145/2442516.2442524
 *
 * @tparam T The type of elements stored in the deque.
 */
template <typename T> class Deque {
public:
  explicit Deque(std::int64_t capacity = 1024);

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
  /* Determines if the element type T satisfies the conditions for optimization
   * (no dynamic memory allocation/deallocation) */
  static constexpr bool no_alloc = std::conjunction_v<
      std::is_trivially_copyable<T>, std::is_copy_constructible<T>,
      std::is_move_constructible<T>, std::is_copy_assignable<T>,
      std::is_move_assignable<T>, std::is_trivially_destructible<T>>;

  using buffer_t =
      internal::CircularBuffer<std::conditional_t<no_alloc, T, T *>>;

  /* Atomic counters to keep track of top and bottom indices of the deque */
  std::atomic<std::int64_t> top_;
  std::atomic<std::int64_t> bottom_;

  /* Atomic pointer to the buffer used to store the elements of the deque */
  std::atomic<buffer_t *> buffer_;

  /* Vector of discarded buffers for memory management */
  std::vector<std::unique_ptr<buffer_t>> discarded_buffers_;

  /* Constants for memory ordering of atomic operations */
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

  /* If the deque is full, expand the buffer and update the buffer pointer */
  if (bottom - top > buf->capacity() - 1) {
    discarded_buffers_.emplace_back(
        std::exchange(buf, buf->expandAndCopy(top, bottom)));
    buffer_.store(buf, relaxed);
  }

  /* Create a new element in the buffer at the bottom index */
  if constexpr (no_alloc) {
    /* Optimization for trivial types: construct the element directly in place
     */
    buf->set(bottom, {std::forward<Args>(args)...});
  } else {
    /* Allocate memory for the element and construct it in place */
    buf->set(bottom, new T{std::forward<Args>(args)...});
  }

  /* Memory barrier to ensure visibility of changes to other threads */
  std::atomic_thread_fence(release);
  bottom_.store(bottom + 1, relaxed);
}

template <typename T>
std::optional<T>
Deque<T>::pop() noexcept(no_alloc || std::is_nothrow_move_constructible_v<T>) {

  std::int64_t new_bottom = bottom_.load(relaxed) - 1;
  auto *buf = buffer_.load(relaxed);

  /* Update the bottom index and synchronize with other threads */
  bottom_.store(new_bottom, relaxed);

  std::atomic_thread_fence(seq_cst);

  std::int64_t top = top_.load(relaxed);

  /* Check if the deque is not empty */
  if (top <= new_bottom) {
    /*If the deque has a single element, update top and bottom indices
     * atomically */
    if (top == new_bottom) {
      if (!top_.compare_exchange_strong(top, top + 1, seq_cst, relaxed)) {
        bottom_.store(new_bottom + 1, relaxed);
        return std::nullopt;
      }
      bottom_.store(new_bottom + 1, relaxed);
    }

    /* Retrieve the element from the buffer */
    auto t = buf->get(new_bottom);

    /* Return the element based on optimization (with or without dynamic
     * allocation) */
    if constexpr (no_alloc) {
      return t;
    } else {
      std::optional val{std::move(*t)};
      delete t;
      return val;
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

  /* Check if there are elements to steal */
  if (top < bottom) {
    /* Retrieve the element from the buffer */
    auto t = buffer_.load(consume)->get(top);

    /* Try to update the top index atomically, ensuring exclusive access to the
     * element */
    if (!top_.compare_exchange_strong(top, top + 1, seq_cst, relaxed)) {
      return std::nullopt;
    }

    /* Return the element based on optimization (with or without dynamic
     * allocation) */
    if constexpr (no_alloc) {
      return t;
    } else {
      std::optional val{std::move(*t)};
      delete t;
      return val;
    }

  } else {
    return std::nullopt;
  }
}

template <typename T> Deque<T>::~Deque() {
  /* Clean up dynamically allocated elements if needed */
  if constexpr (!no_alloc) {
    while (!empty()) {
      pop();
    }
  }
  delete buffer_.load();
}

} // namespace async