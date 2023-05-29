#include <iostream>

#include "async/threadpool.h"
#include "doctest/doctest.h"

TEST_CASE("threadpool.IdentityFunction") {
  async::ThreadPool pool(2);

  auto result = pool.submit([](int x) { return x; }, 100);

  REQUIRE(result.get() == 100);
}

TEST_CASE("threadpool.Destructor") {
  for (int i = 0; i < 10000; i++) {
    async::ThreadPool pool;
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

TEST_CASE("threadpool.EmptyTask.1" * doctest::timeout(25)) {
  test_empty_task(1);
}

TEST_CASE("threadpool.EmptyTask.2" * doctest::timeout(25)) {
  test_empty_task(2);
}

TEST_CASE("threadpool.EmptyTask.4" * doctest::timeout(25)) {
  test_empty_task(4);
}

TEST_CASE("threadpool.EmptyTask.8" * doctest::timeout(25)) {
  test_empty_task(8);
}

TEST_CASE("threadpool.EmptyTask.16" * doctest::timeout(25)) {
  test_empty_task(16);
}