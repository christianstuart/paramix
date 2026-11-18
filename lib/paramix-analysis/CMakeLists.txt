if(CMAKE_SOURCE_DIR STREQUAL CMAKE_CURRENT_SOURCE_DIR)
  cmake_minimum_required(VERSION 3.21)
  project(paramix-analysis LANGUAGES CXX)
  set(PARAMIX_ANALYSIS_STANDALONE ON)
else()
  set(PARAMIX_ANALYSIS_STANDALONE OFF)
endif()

add_library(paramix-analysis STATIC
  src/engine.cpp
  src/fields.cpp
  src/fft.cpp
  src/kernel.cpp
  src/logmag.cpp
  src/onset.cpp
  src/phase.cpp
  src/refine.cpp
  src/resampler.cpp
  src/tempo.cpp
  src/window.cpp
)

target_include_directories(paramix-analysis
  PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}/include
)

target_compile_features(paramix-analysis PUBLIC cxx_std_20)

# The port mirrors float32 rounding step for step. Contraction (FMA) and fast
# math would change the results, so both are disabled explicitly.
if(MSVC)
  target_compile_options(paramix-analysis PRIVATE /fp:precise)
else()
  target_compile_options(paramix-analysis PRIVATE -ffp-contract=off -fno-fast-math)
endif()

option(PARAMIX_ANALYSIS_BUILD_TESTS
  "Build the paramix-analysis unit tests" ON)

if(PARAMIX_ANALYSIS_BUILD_TESTS AND (PARAMIX_ANALYSIS_STANDALONE OR BUILD_TESTING))
  if(PARAMIX_ANALYSIS_STANDALONE)
    find_package(GTest QUIET)
    if(GTest_FOUND)
      enable_testing()
    endif()
  endif()

  if(TARGET GTest::gtest OR GTest_FOUND)
    include(GoogleTest)
    enable_testing()
    add_executable(paramix-analysis-test
      test/engine_test.cpp
      test/frontend_test.cpp
      test/resampler_test.cpp
      test/tempo_phase_test.cpp
    )
    target_link_libraries(paramix-analysis-test
      PRIVATE
        paramix-analysis
        GTest::gtest
        GTest::gtest_main
    )
    target_include_directories(paramix-analysis-test PRIVATE
      ${CMAKE_CURRENT_SOURCE_DIR}/src
    )

    # When built as part of Paramix, also exercise the Qt plugin adapter and
    # the additive downbeat serialization through the real Mixxx types.
    if(TARGET mixxx-lib)
      target_sources(paramix-analysis-test PRIVATE test/mixxx_integration_test.cpp)
      target_link_libraries(paramix-analysis-test PRIVATE mixxx-lib)
      target_compile_definitions(paramix-analysis-test PRIVATE
        PARAMIX_ANALYSIS_WITH_MIXXX=1
      )
    endif()
    gtest_discover_tests(paramix-analysis-test
      DISCOVERY_MODE PRE_TEST
      DISCOVERY_TIMEOUT 60
    )
  elseif(PARAMIX_ANALYSIS_STANDALONE)
    message(STATUS "paramix-analysis: GTest not found, unit tests are disabled")
  endif()
endif()
