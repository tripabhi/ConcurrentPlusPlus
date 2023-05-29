project(${CMAKE_PROJECT_NAME})

find_package(Threads)

add_library(async INTERFACE ${ASYNC_INTERFACE_HEADERS})

set(ASYNC_INTERFACE_HEADERS
    async/deque.h async/internal/buffer.h async/internal/xoroshiro128starstar.h
    async/mutex.h async/sem.h async/threadpool.h)

target_link_libraries(async INTERFACE ${CMAKE_THREAD_LIBS_INIT}
                                      function2::function2)

target_compile_features(async INTERFACE cxx_std_20)

# target_compile_options(async INTERFACE -lrt)

target_include_directories(
  async INTERFACE $<INSTALL_INTERFACE:${CMAKE_INSTALL_INCLUDDIR}>
                  $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}>)

# Install

install(
  TARGETS async
  EXPORT ${CMAKE_PROJECT_NAME}
  DESTINATION ${CMAKE_INSTALL_LIBDIR}/async/cmake)

install(FILES ${ASYNC_INTERFACE_HEADERS}
        DESTINATION ${CMAKE_INSTALL_INCLUDDIR}/async)
