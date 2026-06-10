add_executable(PetLogicTest
  PetLogicTest.cpp
  ${REPO_ROOT}/lib/Pet/PetDecayEngine.cpp
  ${REPO_ROOT}/lib/Pet/PetEvolution.cpp
)

target_include_directories(PetLogicTest PRIVATE
  ${CMAKE_CURRENT_SOURCE_DIR}
  ${REPO_ROOT}/lib/Pet
)

target_link_libraries(PetLogicTest PRIVATE
  inkpoint_test_common
  GTest::gtest_main
)

gtest_discover_tests(PetLogicTest)
