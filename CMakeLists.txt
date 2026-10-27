cmake_minimum_required(VERSION 3.21)
if(POLICY CMP0091)
    cmake_policy(SET CMP0091 NEW)
endif()

project(PakalCore CXX)

# The engine's core alone: PakalCore (no backends; the dummy systems take their place) and CoreTests. The full engine,
# with its backends, genres and examples, is on the master branch.

if(MSVC)
    set(CMAKE_MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>DLL")
endif()

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin)
set(CMAKE_LIBRARY_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/lib)
set(CMAKE_ARCHIVE_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/lib)

enable_testing()

# A config.h compatibility header for case-sensitive file systems
if(NOT WIN32)
    file(WRITE "${CMAKE_BINARY_DIR}/config.h" "#include \"Config.h\"\n")
endif()

include(${CMAKE_SOURCE_DIR}/cmake/PakalCore.cmake)
