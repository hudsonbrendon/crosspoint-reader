add_executable(FlashcardCsvTest
  FlashcardCsvTest.cpp
  ${REPO_ROOT}/src/activities/flashcard/FlashcardCsv.cpp
)
target_include_directories(FlashcardCsvTest PRIVATE
  ${CMAKE_CURRENT_SOURCE_DIR}
  ${REPO_ROOT}/src/activities/flashcard
)
target_link_libraries(FlashcardCsvTest PRIVATE
  inkpoint_test_common
  GTest::gtest_main
)
gtest_discover_tests(FlashcardCsvTest)
