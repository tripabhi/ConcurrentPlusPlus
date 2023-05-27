#include "doctest/doctest.h"
#include <async/deque.h>

TEST_CASE("Simple Single-Threaded Deque Operations") {
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