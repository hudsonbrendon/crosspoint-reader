add_executable(IndentDecisionTest
  IndentDecisionTest.cpp
)

target_include_directories(IndentDecisionTest PRIVATE
  ${CMAKE_CURRENT_SOURCE_DIR}
)

target_link_libraries(IndentDecisionTest PRIVATE
  inkpoint_test_common
  GTest::gtest_main
)

gtest_discover_tests(IndentDecisionTest)
