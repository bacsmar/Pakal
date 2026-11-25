# A game in a repository of its own, with the engine as a submodule (docs/GAME_REPOSITORY.md). Copy this folder to the
# new repository's root, add the engine at extern/Pakal (git submodule add <engine's url> extern/Pakal) and build:
#
#   cmake --preset win64
#   cmake --build build-win64
#   build-win64\bin\PakalPlayer.exe ExampleModule
#
# tests/GameRepositoryTests.cmake configures this folder against the engine's own tree (PAKAL_SOURCE) on every ctest run.
cmake_minimum_required(VERSION 3.21)
if(POLICY CMP0091)
    cmake_policy(SET CMP0091 NEW)   # the engine picks the C runtime by CMAKE_MSVC_RUNTIME_LIBRARY
endif()
project(ExampleGame CXX C)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
# the game's module next to PakalPlayer, which loads it from its own folder
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin)
set(CMAKE_LIBRARY_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/lib)
set(CMAKE_ARCHIVE_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/lib)

# The engine: its libraries (Pakal::Pakal, Pakal::TopDownView...), PakalPlayer and pakal_add_game_module(). Its examples,
# tests and tools stay out (PAKAL_BUILD_EXAMPLES, PAKAL_BUILD_TESTS, PAKAL_BUILD_TOOLS: OFF when it is not the top project).
set(PAKAL_SOURCE ${CMAKE_CURRENT_SOURCE_DIR}/extern/Pakal CACHE PATH "The engine's tree")
add_subdirectory(${PAKAL_SOURCE} pakal)

pakal_add_game_module(ExampleModule
    SOURCES source/ExampleModule.cpp
    LINKS Pakal::TopDownView
    DATA data example)
# a module is loaded by the player: build them together
add_dependencies(ExampleModule PakalPlayer)
