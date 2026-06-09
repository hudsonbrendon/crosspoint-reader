add_executable(ReadingStatsTest
  ReadingStatsTest.cpp
  ${REPO_ROOT}/lib/ReadingStats/ReadingStats.cpp
  ${REPO_ROOT}/lib/ReadingStats/StatsFormat.cpp
)

target_include_directories(ReadingStatsTest PRIVATE
  ${CMAKE_CURRENT_SOURCE_DIR}
  ${REPO_ROOT}/lib/ReadingStats
)

target_link_libraries(ReadingStatsTest PRIVATE
  inkpoint_test_common
  GTest::gtest_main
)

gtest_discover_tests(ReadingStatsTest)
