cmake_minimum_required(VERSION 3.15)

project(
  ConcurrentPlusPlus
  VERSION 1.0.0
  LANGUAGES CXX)

set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

set(CMAKE_CXX_STANDARD 20)

# Build include directory
add_subdirectory(include)
add_subdirectory(function2)

option(BUILD_TESTS "Flag to build tests" ON)

if(BUILD_TESTS)
  add_subdirectory(tests)
endif()

# Install
install(FILES cmake/async-config.cmake
        DESTINATION ${CMAKE_INSTALL_LIBDIR}/cmake)

install(EXPORT ${CMAKE_PROJECT_NAME} DESTINATION ${CMAKE_INSTALL_LIBDIR}/cmake)
