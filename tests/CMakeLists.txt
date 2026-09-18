include(FetchContent)
FetchContent_Declare(
  Catch2
  GIT_REPOSITORY https://github.com/catchorg/Catch2.git
  GIT_TAG        v3.7.1
)
FetchContent_MakeAvailable(Catch2)

add_executable(ampcore_tests
  test_fuzz.cpp
)
target_link_libraries(ampcore_tests PRIVATE ampcore_core Catch2::Catch2WithMain)

include(CTest)
include(Catch)
catch_discover_tests(ampcore_tests)
