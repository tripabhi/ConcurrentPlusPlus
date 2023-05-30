#include <iostream>

#include "async/threadpool.h"
#include "doctest/doctest.h"

TEST_CASE("threadpool.Destructor") {
  for (int i = 0; i < 10000; i++) {
    async::ThreadPool pool;
  }
}

void test_identity_function(std::size_t nthreads) {
  std::vector<std::future<int>> futures;

  {
    async::ThreadPool pool(nthreads);
    for (int i = 0; i < 100000; i++) {
      futures.push_back(pool.submit([](int x) { return x; }, i));
    }
  }

  for (int i = 0; i < 100000; i++) {
    REQUIRE(futures[i].get() == i);
  }
}

void test_empty_task(std::size_t nthreads) {
  std::vector<std::future<void>> futures;

  {
    async::ThreadPool pool(nthreads);
    for (std::size_t i = 0; i < (1ul << 21); i++) {
      futures.push_back(pool.submit([]() {}));
    }
  }

  for (auto &&f : futures) {
    REQUIRE(f.valid());
    f.wait();
  }
}

TEST_CASE("threadpool.EmptyTask.1Thread" * doctest::timeout(25)) {
  test_empty_task(1);
}

TEST_CASE("threadpool.EmptyTask.2Threads" * doctest::timeout(25)) {
  test_empty_task(2);
}

TEST_CASE("threadpool.EmptyTask.4Threads" * doctest::timeout(25)) {
  test_empty_task(4);
}

TEST_CASE("threadpool.EmptyTask.8Threads" * doctest::timeout(25)) {
  test_empty_task(8);
}

TEST_CASE("threadpool.EmptyTask.16Threads" * doctest::timeout(25)) {
  test_empty_task(16);
}

TEST_CASE("threadpool.IdentityFunction.1Thread" * doctest::timeout(25)) {
  test_identity_function(1);
}

TEST_CASE("threadpool.IdentityFunction.2Threads" * doctest::timeout(25)) {
  test_identity_function(2);
}

TEST_CASE("threadpool.IdentityFunction.4Threads" * doctest::timeout(25)) {
  test_identity_function(4);
}

TEST_CASE("threadpool.IdentityFunction.8Threads" * doctest::timeout(25)) {
  test_identity_function(8);
}

TEST_CASE("threadpool.IdentityFunction.16Threads" * doctest::timeout(25)) {
  test_identity_function(16);
}

void test_with_varying_wait_periods(std::size_t nthreads) {
  std::vector<std::future<void>> futures;

  {
    async::ThreadPool pool(nthreads);
    for (int i = 0; i < nthreads * 10; i++) {
      futures.emplace_back(pool.submit([i]() {
        std::this_thread::sleep_for(std::chrono::milliseconds(i * 10));
      }));
    }
  }

  for (auto &fut : futures) {
    REQUIRE(fut.valid());
  }
}

TEST_CASE("threadpool.VaryingWait.1Thread") {
  test_with_varying_wait_periods(1);
}

TEST_CASE("threadpool.VaryingWait.2Threads") {
  test_with_varying_wait_periods(2);
}

TEST_CASE("threadpool.VaryingWait.4Threads") {
  test_with_varying_wait_periods(4);
}

TEST_CASE("threadpool.VaryingWait.8Threads") {
  test_with_varying_wait_periods(8);
}

TEST_CASE("threadpool.VaryingWait.16Threads") {
  test_with_varying_wait_periods(16);
}