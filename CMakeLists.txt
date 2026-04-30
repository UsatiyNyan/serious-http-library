cmake_minimum_required(VERSION 3.21)

include(FetchContent)
FetchContent_Declare(
        serious-cmake-library
        GIT_REPOSITORY https://github.com/UsatiyNyan/serious-cmake-library.git
        GIT_TAG main
        GIT_SHALLOW TRUE
)
FetchContent_MakeAvailable(serious-cmake-library)

# Project

sl_project_prologue(
        CXX_STANDARD 23
        C_STANDARD 17)

project(
        serious-http-library
        VERSION 0.0.0
        DESCRIPTION "TODO"
        HOMEPAGE_URL "TODO"
        LANGUAGES C CXX)

add_library(${PROJECT_NAME} STATIC
    src/v1/detail/percent_encoding.cpp
    src/v1/detail/strings.cpp
    src/v1/detail/target.cpp
    src/v1/deserialize/request.cpp
)
add_library(sl::http ALIAS ${PROJECT_NAME})

target_include_directories(${PROJECT_NAME} PUBLIC include)

add_subdirectory(dependencies)

# Tests and examples

if (NOT PROJECT_IS_TOP_LEVEL)
    return()
endif ()

sl_compiler_warnings(${PROJECT_NAME} WARNINGS_AS_ERRORS)

include(CTest)
if (BUILD_TESTING)
    add_subdirectory(test)
endif ()

add_subdirectory(examples)
