# ConcurrentPlusPlus
ConcurrentPlusPlus is a C++ library that helps you write parallel programs. The library currently provides the following implementations:
- `deque.h` - A fast, lock-free work stealing Deque implementation.
- `threadpool.h` - A simple threadpool that can execute tasks in parallel.

# Build
To build the project:
``` bash
mkdir build && cd build
cmake ..
make
```
Once the project is built, you can run the test suite from `build/tests` folder.
You can either run the `tests` executable, or you can run `ctest`.

# Usage
To add this library to your project, you can use [CPM.cmake](https://github.com/cpm-cmake/CPM.cmake) to use our project like this:

``` cmake
CPMAddPackage("gh:tripabhi/ConcurrentPlusPlus#1.0.0")

target_link_libraries(${TARGET} ConcurrentPlusPlus::async)
```

Once the library is added, you can use the `ThreadPool` class like this:
``` cpp
#include <async/threadpool.h>

int multiply(int a, int b) { return a * b; }

int main() {
  async::ThreadPool pool(4);
  std::future<int> future = pool.submit(multiply, 4, 5);
  assert(future.get() == 20);
}
```



