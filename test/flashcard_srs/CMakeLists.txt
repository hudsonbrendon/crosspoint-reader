add_executable(FlashcardSrsTest
  FlashcardSrsTest.cpp
  ${REPO_ROOT}/src/activities/flashcard/FlashcardSrs.cpp
)
target_include_directories(FlashcardSrsTest PRIVATE
  ${CMAKE_CURRENT_SOURCE_DIR}
  ${REPO_ROOT}/src/activities/flashcard
)
target_link_libraries(FlashcardSrsTest PRIVATE
  inkpoint_test_common
  GTest::gtest_main
)
gtest_discover_tests(FlashcardSrsTest)
