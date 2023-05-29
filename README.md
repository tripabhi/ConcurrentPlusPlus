# ConcurrentPlusPlus
ConcurrentPlusPlus is a C++ library that helps you write parallel programs. The library currently provides the following:
- `deque.h` - A fast, lock-free work stealing Deque implementation.
- `threadpool.h` - A simple threadpool that can execute tasks in parallel.

# Usage
The `ThreadPool` class can be used like:
```
#include <async/threadpool.h>

int multiply(int a, int b) { return a * b; }

int main() {
  async::ThreadPool pool(4);
  std::future<int> future = pool.submit(multiply, 4, 5);
  assert(future.get() == 20);
}
```

# Build
To build the project:
```
mkdir build && cd build
cmake ..
make
```
