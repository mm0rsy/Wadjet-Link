# Scenario module CMakeLists.txt
# This file is included from the parent src/CMakeLists.txt

# Sources are added directly to the parent wadjet library
set(SCENARIO_SOURCES
    ${CMAKE_CURRENT_SOURCE_DIR}/yaml_parser.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/json_parser.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/runner.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/report.cpp
    PARENT_SCOPE
)
