cmake_minimum_required(VERSION 3.23)

project(shadegenerator VERSION 1.0.0 LANGUAGES CXX)

include(FetchContent)

FetchContent_Declare(
    CLI11
    GIT_REPOSITORY https://github.com/CLIUtils/CLI11.git
    GIT_TAG v2.7.2
    GIT_SHALLOW TRUE
)
FetchContent_MakeAvailable(CLI11)

set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)
set(CMAKE_MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>" CACHE STRING "" FORCE)

find_package(Doxygen QUIET)
if(Doxygen_FOUND)
    configure_file(Doxyfile.in Doxyfile @ONLY)
    add_custom_target(docs
        COMMAND "${DOXYGEN_EXECUTABLE}" "${CMAKE_CURRENT_BINARY_DIR}/Doxyfile"
        WORKING_DIRECTORY "${CMAKE_CURRENT_BINARY_DIR}"
        BYPRODUCTS "${CMAKE_CURRENT_BINARY_DIR}/docs/html/index.html"
        COMMENT "Generating API documentation"
        VERBATIM
    )
else()
    message(STATUS "Doxygen was not found; the docs target is unavailable")
endif()

set(SHADEGENERATOR_GAME "CS2" CACHE STRING "Target game: CS2, DOTA2, or DEADLOCK")
set_property(CACHE SHADEGENERATOR_GAME PROPERTY STRINGS CS2 DOTA2 DEADLOCK)

string(TOUPPER "${SHADEGENERATOR_GAME}" SHADEGENERATOR_GAME)
set(SHADEGENERATOR_SUPPORTED_GAMES CS2 DOTA2 DEADLOCK)

if(NOT SHADEGENERATOR_GAME IN_LIST SHADEGENERATOR_SUPPORTED_GAMES)
    message(FATAL_ERROR
        "Unsupported SHADEGENERATOR_GAME='${SHADEGENERATOR_GAME}'. "
        "Expected one of: CS2, DOTA2, DEADLOCK")
endif()

add_library(shadegenerator-game-config INTERFACE)
target_compile_definitions(shadegenerator-game-config INTERFACE
    "SHADE_GAME_${SHADEGENERATOR_GAME}=1"
)

add_library(shadegenerator-warnings INTERFACE)
if(CMAKE_CXX_COMPILER_ID MATCHES "Clang")
    target_compile_options(shadegenerator-warnings INTERFACE
        -Wall
        -Wextra
        -Wpedantic

        # The bundled Valve headers intentionally use legacy Microsoft and GNU
        # extensions. These diagnostics do not describe first-party code.
        -Wno-c2y-extensions
        -Wno-c99-extensions
        -Wno-deprecated-copy-with-user-provided-copy
        -Wno-gnu-anonymous-struct
        -Wno-gnu-null-pointer-arithmetic
        -Wno-ignored-qualifiers
        -Wno-language-extension-token
        -Wno-macro-redefined
        -Wno-nested-anon-types
        -Wno-unused-value
    )
endif()

add_subdirectory(shadegenerator/sdk)
target_link_libraries(shadegenerator-sdk INTERFACE shadegenerator-game-config)
add_subdirectory(shadegenerator/utils)
add_subdirectory(shadegenerator/tools)
add_subdirectory(shadegenerator/codegen)
add_subdirectory(shadegenerator/game)

add_executable(shadegenerator
    shadegenerator/main.cpp
    shadegenerator/generation.cpp
)
target_compile_definitions(shadegenerator PRIVATE
    SHADEGENERATOR_VERSION="${PROJECT_VERSION}"
)
target_link_libraries(shadegenerator PRIVATE
    shadegenerator-sdk
    shadegenerator-utils
    shadegenerator-tools
    shadegenerator-codegen
    shadegenerator-game
    CLI11::CLI11
    shadegenerator-warnings
)

option(SHADEGENERATOR_CLANG_TIDY "Run clang-tidy during the build" OFF)
if(SHADEGENERATOR_CLANG_TIDY)
    find_program(SHADEGENERATOR_CLANG_TIDY_EXE NAMES clang-tidy REQUIRED)
    foreach(target IN ITEMS
        shadegenerator
        shadegenerator-codegen
        shadegenerator-game
        shadegenerator-tools
    )
        set_property(TARGET "${target}" PROPERTY CXX_CLANG_TIDY "${SHADEGENERATOR_CLANG_TIDY_EXE}")
    endforeach()
endif()
