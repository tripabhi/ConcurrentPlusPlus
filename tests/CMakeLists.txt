project(${CMAKE_PROJECT_NAME})

include(../cmake/CPM.cmake)

CPMAddPackage("gh:onqtam/doctest#2.4.7")

find_package(doctest REQUIRED)

include(${doctest_SOURCE_DIR}/scripts/cmake/doctest.cmake)

enable_testing()

file(GLOB sources CONFIGURE_DEPENDS ${CMAKE_CURRENT_SOURCE_DIR}/*.cpp)
add_executable(tests "${sources}")

target_link_libraries(tests PUBLIC ${CMAKE_THREAD_LIBS_INIT} async
                                   doctest::doctest)

target_compile_definitions(
  tests
  PUBLIC $<$<COMPILE_LANG_AND_ID:CXX,MSVC>:DOCTEST_CONFIG_USE_STD_HEADERS>)

doctest_discover_tests(tests)
