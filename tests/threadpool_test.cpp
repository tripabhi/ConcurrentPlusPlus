#include "doctest/doctest.h"
#include <async/threadpool.h>
#include <iostream>

TEST_CASE("threadpool.SimpleIdentityFunction") {
  async::ThreadPool pool(2);
  // Run an Identity Function
  auto future = pool.submit([](int x) { return x; }, 100);
}
