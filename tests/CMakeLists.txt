include(FetchContent)
FetchContent_Declare(
  Catch2
  GIT_REPOSITORY https://github.com/catchorg/Catch2.git
  GIT_TAG        v3.7.1
)
FetchContent_MakeAvailable(Catch2)

add_executable(ampcore_tests
  test_fuzz.cpp
  test_file_input.cpp
  test_graph_registry.cpp
)
target_link_libraries(ampcore_tests PRIVATE ampcore_core Catch2::Catch2WithMain)

# the C API as a C caller sees it: only the shared library, no module imports
add_executable(ampcore_capi_tests test_capi.cpp)
target_link_libraries(ampcore_capi_tests PRIVATE ampcore_capi Catch2::Catch2WithMain)

include(CTest)
include(Catch)
catch_discover_tests(ampcore_tests)
catch_discover_tests(ampcore_capi_tests)
