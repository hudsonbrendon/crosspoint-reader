add_executable(HtmlToTextTest
  HtmlToTextTest.cpp
  ${REPO_ROOT}/lib/RssParser/HtmlToText.cpp
)

target_include_directories(HtmlToTextTest PRIVATE
  ${CMAKE_CURRENT_SOURCE_DIR}
  ${REPO_ROOT}/lib/RssParser
)

target_link_libraries(HtmlToTextTest PRIVATE
  inkpoint_test_common
  GTest::gtest_main
)

gtest_discover_tests(HtmlToTextTest)
