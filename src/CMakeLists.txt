cmake_minimum_required(VERSION 4.2)
project(stdred LANGUAGES C)

find_package(Python3 COMPONENTS Interpreter REQUIRED)
find_library(UTIL_LIBRARY util)
find_program(EXPECT_EXECUTABLE expect)

add_executable(emit_fixture "${PROJECT_SOURCE_DIR}/../tests/fixtures/emit.c")
add_executable(stdred stdred.c)
set_target_properties(
  emit_fixture
  stdred
  PROPERTIES C_STANDARD 99 C_STANDARD_REQUIRED ON
)

if (UTIL_LIBRARY)
  target_link_libraries(stdred PRIVATE ${UTIL_LIBRARY})
endif()

install(TARGETS stdred DESTINATION bin)
install(FILES "${PROJECT_SOURCE_DIR}/../stdred.plugin.zsh" DESTINATION share/stdred)

include(CTest)

add_test(
  NAME plugin-tests
  COMMAND zsh "${PROJECT_SOURCE_DIR}/../tests/plugin_tests.zsh" "${PROJECT_SOURCE_DIR}/../stdred.plugin.zsh"
)

add_test(
  NAME helper-tests
  COMMAND
    ${Python3_EXECUTABLE}
    "${PROJECT_SOURCE_DIR}/../tests/helper_tests.py"
    "$<TARGET_FILE:stdred>"
    "$<TARGET_FILE:emit_fixture>"
)

if (EXPECT_EXECUTABLE)
  add_test(
    NAME interactive-tests
    COMMAND
      ${EXPECT_EXECUTABLE}
      "${PROJECT_SOURCE_DIR}/../tests/interactive.expect"
      "${PROJECT_SOURCE_DIR}/../stdred.plugin.zsh"
      "${CMAKE_BINARY_DIR}"
      "${CMAKE_BINARY_DIR}/interactive-marker.txt"
  )
endif()
