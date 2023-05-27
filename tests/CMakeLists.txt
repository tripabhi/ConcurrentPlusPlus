project(${CMAKE_PROJECT_NAME})

include(../cmake/CPM.cmake)

cpmaddpackage("gh:onqtam/doctest#2.4.6")

find_package(doctest REQUIRED)

include(${doctest_SOURCE_DIR}/scripts/cmake/doctest.cmake)

enable_testing()

file(GLOB sources CONFIGURE_DEPENDS ${CMAKE_CURRENT_SOURCE_DIR}/*.cpp)
add_executable(tests "${sources}")

target_link_libraries(tests PRIVATE async doctest::doctest)

# doctest_discover_tests(tests)
