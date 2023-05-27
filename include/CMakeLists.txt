project(${CMAKE_PROJECT_NAME})

add_library(async INTERFACE ${ASYNC_INTERFACE_HEADERS})

set(ASYNC_INTERFACE_HEADERS async/deque.h async/internal/buffer.h)

target_link_libraries(async INTERFACE pthread)

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
