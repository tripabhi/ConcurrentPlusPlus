#pragma once

#include "async/sem.h"
#include <atomic>
#include <memory>

namespace async {
/**
 * @brief A lightweight implementation of mutex using Semaphore
 *
 */
class Mutex {
public:
  void lock() {
    if (contention_.fetch_add(1, std::memory_order_acquire) > 0) {
      sem_.wait();
    }
  }

  void unlock() {
    if (contention_.fetch_sub(1, std::memory_order_release) > 1) {
      sem_.signal();
    }
  }

private:
  std::atomic<int> contention_;
  DefaultSemaphoreType sem_;
};
} // namespace async