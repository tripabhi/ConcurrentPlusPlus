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

template <class T> std::decay_t<T> decay_copy(T &&v) {
  return std::forward<T>(v);
}

template <typename... Args, typename F>
auto bindFunctionToArguments(F &&f, Args &&... args) {
  return [f = decay_copy(std::forward<F>(f)),
          ... args = decay_copy(
              std::forward<Args>(args))]() mutable -> decltype(auto) {
    return std::invoke(std::move(f), std::move(args)...);
  };
}

template <std::invocable F> class Task {
public:
  // Stores a copy of the function
  Task(F fn) : callable_(std::move(fn)) {}

  std::future<std::invoke_result_t<F>> get_future() {
    return promise_.get_future();
  }

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
  std::promise<std::invoke_result_t<F>> promise_;
  F callable_;
};

} // namespace internal

class ThreadPool {
public:
  explicit ThreadPool(
      std::size_t nthreads = std::thread::hardware_concurrency())
      : queues_(nthreads) {
    for (std::size_t i = 0; i < nthreads; ++i) {
      threads_.emplace_back([&, id = i](std::stop_token token) {
        prng::jump();
        do {
          queues_[id].sem.wait();
          std::size_t spin_count = 0;
          do {
            std::size_t t = spin_count++ < 100 || !queues_[id].dq.empty()
                                ? id
                                : prng::next() % queues_.size();

            if (std::optional fetched_task = queues_[t].dq.steal()) {
              pending_task_count_.fetch_sub(1, std::memory_order_release);
              std::invoke(std::move(*fetched_task));
            }
          } while (pending_task_count_.load(std::memory_order_acquire) > 0);

        } while (!token.stop_requested());
      });
    }
  }

  template <typename... Args, typename F>
  std::future<std::invoke_result_t<std::decay_t<F>, std::decay_t<Args>...>>
  submit(F &&f, Args &&... args);

  ~ThreadPool();

private:
  struct TaskQueue {
    DefaultSemaphoreType sem{0};
    Deque<fu2::unique_function<void() &&>> dq;
  };

  std::atomic<std::int64_t> pending_task_count_;
  std::size_t rotating_index_ = 0;
  std::vector<TaskQueue> queues_;
  std::vector<std::jthread> threads_;

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
