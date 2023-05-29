#pragma once

#include <atomic>
#include <cassert>
#include <concepts>
#include <cstdint>
#include <functional>
#include <future>
#include <ratio>
#include <thread>
#include <type_traits>
#include <utility>

#include "async/deque.h"
#include "function2/function2.hpp"
#include <async/internal/xoroshiro128starstar.h>
#include <async/sem.h>

namespace async {

namespace internal {

/**
 * @brief Creates a decayed copy of the given value.
 *
 * @tparam T The type of the value.
 * @param v The value to be copied.
 * @return std::decay_t<T> The decayed copy of the value.
 */
template <class T> std::decay_t<T> decay_copy(T &&v) {
  return std::forward<T>(v);
}

/**
 * @brief Binds a function to its arguments and returns a callable
 * object/lambda.
 *
 * @tparam Args Variadic template parameter pack for the types of arguments.
 * @tparam F The type of the function to be bound.
 * @param f The function to be bound.
 * @param args The arguments to be bound to the function.
 * @return Callable The callable object that encapsulates the function and its
 * arguments.
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
 * @brief Represents a task that can be executed asynchronously. Similar to
 * std::packaged_task<T>
 *
 * The Task class stores a callable object (function) and provides a mechanism
 * to execute it and obtain the result asynchronously through a future object.
 *
 * @note The Task class assumes that the callable object is invocable and
 * provides a valid return type.
 */
template <std::invocable F> class Task {
public:
  /**
   * @brief Constructs a Task with the specified callable object.
   *
   * @param fn The callable object (function) to be stored.
   */
  Task(F fn) : callable_(std::move(fn)) {}

  /**
   * @brief Obtains the future object associated with the task result.
   *
   * @return std::future<std::invoke_result_t<F>> The future object associated
   * with the task result.
   */
  std::future<std::invoke_result_t<F>> get_future() {
    return promise_.get_future();
  }

  /**
   * @brief Invokes the stored callable object and sets the promise value or
   * exception. This allows for functions with void and non-void return types to
   * be wrapped by the same class.
   *
   * The operator() invokes the stored callable object and sets the promise
   * value or exception based on the result of the invocation.
   */
  void operator()() && {
    try {
      if constexpr (!std::is_same_v<void, std::invoke_result_t<F>>) {
        promise_.set_value(std::invoke(std::move(callable_)));
      } else {
        std::invoke(std::move(callable_));
        promise_.set_value();
      }
    } catch (...) {
      promise_.set_exception(std::current_exception());
    }
  }

private:
  std::promise<std::invoke_result_t<F>>
      promise_; /* Promise object for setting the result or exception */
  F callable_;  /* Stored callable object (function) */
};

} // namespace internal

/**
 * @brief A thread pool implementation for executing tasks in parallel.
 *
 * The ThreadPool class provides a mechanism to execute tasks in parallel using
 * a pool of threads. It distributes tasks across multiple threads and ensures
 * efficient utilization of available resources.
 *
 * The tasks submitted to the thread pool are executed asynchronously, and the
 * result of each task can be obtained through the returned `std::future`
 * object.
 *
 * @note The ThreadPool class is not copyable or movable.
 */
class ThreadPool {
public:
  /**
   * @brief Constructs a ThreadPool with a specified number of threads.
   *
   * @param nthreads The number of threads in the thread pool. Defaults to the
   * number of hardware threads available.
   */
  explicit ThreadPool(
      std::size_t nthreads = std::thread::hardware_concurrency())
      : queues_(nthreads) {
    for (std::size_t i = 0; i < nthreads; ++i) {
      threads_.emplace_back([&, id = i](std::stop_token token) {
        /* Worker thread routine */
        prng::jump(); /* Creates a large non-overlapping sequence to generate
                         random numbers */
        do {
          /* Wait for task to pushed and worker to be signaled. */
          queues_[id].sem.wait();
          std::size_t spin_count = 0;
          do {
            /* Decide whether to work on one's own queue or from a random
             * worker. */
            std::size_t slot = spin_count++ < 100 || !queues_[id].dq.empty()
                                   ? id
                                   : prng::next() % queues_.size();

            if (std::optional fetched_task = queues_[slot].dq.steal()) {
              pending_task_count_.fetch_sub(1, std::memory_order_release);
              std::invoke(std::move(*fetched_task));
            }

            /* Work until all tasks are finished */
          } while (pending_task_count_.load(std::memory_order_acquire) > 0);

        } while (!token.stop_requested());
      });
    }
  }

  /**
   * @brief Submits a task to the thread pool for execution.
   *
   * @tparam Args Variadic template parameter pack for the types of arguments
   * passed to the task function.
   * @tparam F The type of the task function.
   * @param f The task function to be executed.
   * @param args The arguments to be passed to the task function.
   * @return std::future<std::invoke_result_t<std::decay_t<F>,
   * std::decay_t<Args>...>> The future object associated with the task result.
   */
  template <typename... Args, typename F>
  std::future<std::invoke_result_t<std::decay_t<F>, std::decay_t<Args>...>>
  submit(F &&f, Args &&... args);

  /**
   * @brief Destructor.
   *
   * Stops the threads in the thread pool and waits for them to join before the
   * destruction of the ThreadPool object.
   */
  ~ThreadPool();

private:
  /**
   * @brief Internal structure for storing a task queue associated with a
   * thread.
   */
  struct TaskQueue {
    DefaultSemaphoreType sem{0}; // Semaphore for thread synchronization
    Deque<fu2::unique_function<void() &&>> dq; // Deque to store tasks
  };

  std::atomic<std::int64_t> pending_task_count_; // Counter for pending tasks
  std::size_t rotating_index_ = 0;    // Index for rotating task distribution
  std::vector<TaskQueue> queues_;     // Vector of task queues
  std::vector<std::jthread> threads_; // Vector of worker threads

  /**
   * @brief Pushes a task to the thread pool from an external source.
   *
   * @tparam F The type of the task function.
   * @param f The task function to be executed.
   */
  template <std::invocable F> void externalPush(F &&f);
};

template <typename... Args, typename F>
[[nodiscard]] std::future<
    std::invoke_result_t<std::decay_t<F>, std::decay_t<Args>...>>
ThreadPool::submit(F &&f, Args &&... args) {
  auto task = internal::Task(internal::bindFunctionToArguments(
      std::forward<F>(f), std::forward<Args>(args)...));
  auto future = task.get_future();
  externalPush(std::move(task));
  return future;
}

template <std::invocable F> void ThreadPool::externalPush(F &&f) {
  std::size_t slot = rotating_index_++ % queues_.size();
  pending_task_count_.fetch_add(1, std::memory_order_relaxed);
  queues_[slot].dq.push(std::forward<F>(f));
  queues_[slot].sem.signal();
}

ThreadPool::~ThreadPool() {
  for (auto &t : threads_) {
    t.request_stop();
  }
  for (auto &d : queues_) {
    d.sem.signal();
  }
}
} // namespace async
