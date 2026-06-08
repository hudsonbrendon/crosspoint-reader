add_executable(ReleaseJsonParserTest
  ReleaseJsonParserTest.cpp
  ${REPO_ROOT}/lib/JsonParser/ReleaseJsonParser.cpp
  ${REPO_ROOT}/lib/JsonParser/StreamingJsonParser.cpp
)

target_include_directories(ReleaseJsonParserTest PRIVATE
  ${REPO_ROOT}/lib/JsonParser
)

target_link_libraries(ReleaseJsonParserTest PRIVATE
  inkpoint_test_common
  GTest::gtest_main
)

gtest_discover_tests(ReleaseJsonParserTest)
