#include "doctest/doctest.h"
#include <async/deque.h>
#include <thread>

TEST_CASE("deque.SingleThreadedOperations") {
  async::Deque<int> deque;

  // Check pop() from empty deque
  REQUIRE(!deque.pop());

  // Check push() and pop()
  deque.push(100);

  REQUIRE(*deque.pop() == 100);

  // Check steal()
  REQUIRE(!deque.steal());

  // Check push() and steal()
  deque.push(100);

  REQUIRE(*deque.steal() == 100);
}

TEST_CASE("deque.PushAgainstSteal") {
  async::Deque<int> deque;

  auto &owner = deque;
  auto &thief = deque;

  int ntasks = 1000000;
  int num_threads = 8;
  std::atomic<int> pending(ntasks);

  std::vector<std::thread> thieves;

  for (int i = 0; i < num_threads; i++) {
    thieves.emplace_back([&thief, &pending]() {
      auto &copyQueue = thief;
      while (pending.load(std::memory_order_seq_cst) > 0) {
        std::optional<int> fetched = copyQueue.steal();
        if (fetched) {
          assert((*fetched) == 1);
          pending.fetch_sub(1);
        }
      }
    });
  }

  for (int i = 0; i < ntasks; i++) {
    owner.push(1);
  }

  for (auto &t : thieves) {
    t.join();
  }

  REQUIRE(pending == 0);
}

TEST_CASE("deque.PopAgainstSteal") {
  async::Deque<int> deque;

  auto &owner = deque;
  auto &thief = deque;

  int ntasks = 1000000;
  int num_threads = 4;
  std::atomic<int> pending(ntasks);

  std::vector<std::thread> thieves;

  for (int i = 0; i < ntasks; i++) {
    owner.push(1);
  }

  for (int i = 0; i < num_threads; i++) {
    thieves.emplace_back([&thief, &pending]() {
      auto &copyQueue = thief;
      while (pending.load(std::memory_order_seq_cst) > 0) {
        std::optional<int> fetched = copyQueue.steal();
        if (fetched) {
          assert((*fetched) == 1);
          pending.fetch_sub(1);
        }
      }
    });
  }

  while (pending.load(std::memory_order_seq_cst) > 0) {
    std::optional<int> fetched = owner.pop();
    if (fetched) {
      assert((*fetched) == 1);
      pending.fetch_sub(1);
    }
  }

  for (auto &t : thieves) {
    t.join();
  }

  REQUIRE(pending == 0);
}