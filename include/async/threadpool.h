#pragma once

#include <async/deque.h>
#include <async/internal/utility.h>
#include <async/internal/xoroshiro128starstar.h>
#include <async/sem.h>
#include <concepts>
#include <functional>
#include <future>
#include <thread>
#include <type_traits>
#include <vector>

#include "function2/function2.hpp"

namespace async {
namespace internal {
/**
 * @brief Performs a decayed copy of the universal reference passed to it.
 * The function reduces universal references of types T& and const T& to T.
 *
 * @tparam T Type of reference being copied
 * @param t reference to copy
 * @return std::decay_t<T> Decayed copy of the reference
 */
template <class T> std::decay_t<T> decay_copy(T &&t) {
  return std::forward<T>(t);
}

/**
 * @brief Binds a function to its arguments and returns a callable function with
 * no arguments.
 *
 * @tparam F Function type
 * @tparam Args Variadic Arguments type
 * @param f function
 * @param args variadic arguments
 * @return auto
 */
template <typename... Args, typename F>
auto bindFunctionToArguments(F &&f, Args &&... args) {
  return [f = decay_copy(std::forward<F>(f)),
          ... args = decay_copy(
              std::forward<Args>(args))]() mutable -> decltype(auto) {
    return std::invoke(std::move(f), std::move(args)...);
  };
}

/**
 * @brief Wrapper around a promise to allow ThreadPool tasks that have a void
 * return type.
 *
 * @tparam F Invocable function that takes no arguments and may or may not
 * return a value.
 */
template <std::invocable F> class Task {
public:
  Task(F fn) : callable_(std::move(fn)) {}

  std::future<std::invoke_result_t<F>> get_future() {
    return promise_.get_future();
  }
  /**
   * @brief Overload the () operator so that the class can act as a function.
   * Define the functionality of the () operator based on whether the function
   * has void return type.
   *
   */
  void operator()() && {
    try {
      if constexpr (std::is_same_v<void, std::invoke_result_t<F>>) {
        /* Task has no return type */
        std::invoke(std::move(callable_));
        promise_.set_value();
      } else {
        promise_.set_value(std::invoke(std::move(callable_)));
      }
    } catch (...) {
      promise_.set_exception(std::current_exception());
    }
  }

private:
  std::promise<std::invoke_result_t<F>> promise_;
  F callable_;
};

} // namespace internal

class ThreadPool {

public:
  explicit ThreadPool(
      std::size_t nthreads = std::thread::hardware_concurrency())
      : queues_(nthreads) {
    for (std::size_t i = 0; i < nthreads; i++) {
      threads_.emplace_back([&, id = i](std::stop_token token) {
        prng::jump();
        do {
          queues_[i].sem.wait();

          std::size_t spin_count = 0;

          do {
            /* Acquire your own slot, or else choose a random worker to steal
             * from */
            std::size_t slot = spin_count++ < 100 || !queues_[id].dq.empty()
                                   ? id
                                   : prng::next() % queues_.size();
            if (std::optional fetched_task = queues_[slot].dq.steal()) {
              pending_tasks_.fetch_sub(1, std::memory_order_release);
              std::invoke(std::move(*fetched_task));
            }
          } while (pending_tasks_.load(std::memory_order_acquire) > 0);

        } while (!token.stop_requested());
      });
    }
  }
  void sync();

  template <typename... Args, typename F>
  std::future<std::invoke_result_t<std::decay_t<F>, std::decay_t<Args>...>>
  submit(F &&f, Args &&... args);

  ~ThreadPool() noexcept;

private:
  struct TaskQueue {
    alignas(internal::ALIGNMENT) internal::Semaphore sem{0};
    Deque<fu2::unique_function<void() &&>> dq;
  };

  alignas(internal::ALIGNMENT) std::atomic<std::int64_t> pending_tasks_;
  std::size_t rotating_index_ = 0;

  std::vector<std::jthread> threads_;
  std::vector<TaskQueue> queues_;

  template <std::invocable F> void externalPush(F &&f);
};

template <typename... Args, typename F>
std::future<std::invoke_result_t<std::decay_t<F>, std::decay_t<Args>...>>
ThreadPool::submit(F &&f, Args &&... args) {

  auto task = internal::Task(internal::bindFunctionToArguments(
      std::forward<F>(f), std::forward<Args>(args)...));

  auto future = task.get_future();
  externalPush(std::move(task));

  return future;
}

template <std::invocable F> void ThreadPool::externalPush(F &&f) {
  std::size_t slot = rotating_index_++ % queues_.size();
  /* size_t is uint16_t so overflow is handled */
  pending_tasks_.fetch_add(1, std::memory_order_relaxed);
  queues_[slot].dq.push(std::forward<F>(f));
  queues_[slot].sem.signal();
}

void ThreadPool::sync() {
  for (auto &thread : threads_) {
    thread.request_stop();
  }

  for (auto &tq : queues_) {
    tq.sem.signal();
  }
}

ThreadPool::~ThreadPool() noexcept { sync(); }
} // namespace async