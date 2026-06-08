enable_language(C)

set(EXPAT_SRC
  ${REPO_ROOT}/lib/expat/xmlparse.c
  ${REPO_ROOT}/lib/expat/xmlrole.c
  ${REPO_ROOT}/lib/expat/xmltok.c
)

add_library(expat_rss STATIC ${EXPAT_SRC})
target_include_directories(expat_rss PUBLIC ${REPO_ROOT}/lib/expat)
target_compile_definitions(expat_rss PUBLIC
  XML_GE=0
  XML_CONTEXT_BYTES=1024
)

add_executable(RssParserTest
  RssParserTest.cpp
  ${REPO_ROOT}/lib/RssParser/RssParser.cpp
)

target_include_directories(RssParserTest PRIVATE
  ${CMAKE_CURRENT_SOURCE_DIR}
  ${REPO_ROOT}/lib/RssParser
  ${REPO_ROOT}/lib/expat
)

target_compile_definitions(RssParserTest PRIVATE
  XML_GE=0
  XML_CONTEXT_BYTES=1024
)

target_link_libraries(RssParserTest PRIVATE
  inkpoint_test_common
  GTest::gtest_main
  expat_rss
)

gtest_discover_tests(RssParserTest)
