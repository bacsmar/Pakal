cmake_minimum_required(VERSION 3.10)
project(Pakal)

# Set C++ standard
set(CMAKE_CXX_STANDARD 11)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Set output directories
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin)
set(CMAKE_LIBRARY_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/lib)
set(CMAKE_ARCHIVE_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/lib)

# Create a config.h compatibility header for case-insensitive systems
file(WRITE "${CMAKE_BINARY_DIR}/config.h" "#include \"Config.h\"\n")

# Include directories
include_directories(${CMAKE_BINARY_DIR})
include_directories(${CMAKE_SOURCE_DIR}/source)
include_directories(${CMAKE_SOURCE_DIR}/external/bx/include)
include_directories(${CMAKE_SOURCE_DIR}/external/bgfx/include)
include_directories(${CMAKE_SOURCE_DIR}/external/bimg/include)

# Add source files
file(GLOB_RECURSE PAKAL_SOURCES 
    "${CMAKE_SOURCE_DIR}/source/*.cpp"
    "${CMAKE_SOURCE_DIR}/source/*.h"
    "${CMAKE_SOURCE_DIR}/source/components/*.cpp"
    "${CMAKE_SOURCE_DIR}/source/components/*.h"
)

# Exclude platform-specific files
list(FILTER PAKAL_SOURCES EXCLUDE REGEX ".*/android/.*")
list(FILTER PAKAL_SOURCES EXCLUDE REGEX ".*/win32/.*")
list(FILTER PAKAL_SOURCES EXCLUDE REGEX ".*/unix/.*")
list(FILTER PAKAL_SOURCES EXCLUDE REGEX ".*/examples/.*")

# Platform-specific source files
if(UNIX AND NOT APPLE)
    list(APPEND PAKAL_SOURCES ${CMAKE_SOURCE_DIR}/source/unix/ClockImpl.cpp)
elseif(APPLE)
    list(APPEND PAKAL_SOURCES ${CMAKE_SOURCE_DIR}/source/unix/ClockImpl.cpp)
elseif(WIN32)
    list(APPEND PAKAL_SOURCES ${CMAKE_SOURCE_DIR}/source/win32/ClockImpl.cpp)
endif()

# Create the main library
add_library(Pakal STATIC ${PAKAL_SOURCES})

# Set include paths for the library
target_include_directories(Pakal PUBLIC
    ${CMAKE_BINARY_DIR}
    ${CMAKE_SOURCE_DIR}/source
    ${CMAKE_SOURCE_DIR}/external/bx/include
    ${CMAKE_SOURCE_DIR}/external/bgfx/include
    ${CMAKE_SOURCE_DIR}/external/bimg/include
)

# Find and link required packages - SFML with fallback
find_package(PkgConfig REQUIRED)
pkg_check_modules(SFML REQUIRED sfml-system sfml-window sfml-audio)

if(SFML_FOUND)
    message(STATUS "✓ SFML found via pkg-config")
    target_include_directories(Pakal PRIVATE ${SFML_INCLUDE_DIRS})
    target_link_directories(Pakal PRIVATE ${SFML_LIBRARY_DIRS})
    target_link_libraries(Pakal ${SFML_LIBRARIES})
else()
    message(FATAL_ERROR "❌ SFML not found. Install with: sudo apt-get install libsfml-dev")
endif()

# bgfx library (built with its own makefile, we link the prebuilt .a)
set(BGFX_DIR ${CMAKE_SOURCE_DIR}/external/bgfx)
set(BX_DIR ${CMAKE_SOURCE_DIR}/external/bx)
set(BIMG_DIR ${CMAKE_SOURCE_DIR}/external/bimg)

# Link prebuilt bgfx libraries (built via makefile in Dockerfile)
set(BGFX_BUILD_DIR ${BGFX_DIR}/.build/linux64_gcc/bin)
set(BGFX_LIBS
    ${BGFX_BUILD_DIR}/libbgfxRelease.a
    ${BGFX_BUILD_DIR}/libbxRelease.a
    ${BGFX_BUILD_DIR}/libbimgRelease.a
)

if(EXISTS ${BGFX_BUILD_DIR}/libbgfxRelease.a)
    message(STATUS "✓ Found prebuilt bgfx library")
    target_include_directories(Pakal PUBLIC 
        ${BGFX_DIR}/include
        ${BX_DIR}/include
        ${BX_DIR}/include/compat/linux
        ${BIMG_DIR}/include
    )
    target_link_libraries(Pakal 
        ${BGFX_LIBS}
        dl
    )
else()
    message(WARNING "⚠️  bgfx library not found. Build it with: cd external/bgfx && make linux-gcc-release64")
endif()

# Box2D physics library (optional)
pkg_check_modules(BOX2D box2d)
if(BOX2D_FOUND)
    message(STATUS "✓ Box2D found via pkg-config")
    target_include_directories(Pakal PRIVATE ${BOX2D_INCLUDE_DIRS})
    target_link_directories(Pakal PRIVATE ${BOX2D_LIBRARY_DIRS})
    target_link_libraries(Pakal ${BOX2D_LIBRARIES})
else()
    message(STATUS "⚠️  Box2D not found. Install with: sudo apt-get install libbox2d-dev")
endif()

# Lua (optional, for scripting)
find_package(Lua QUIET)
if(LUA_FOUND)
    target_include_directories(Pakal PRIVATE ${LUA_INCLUDE_DIR})
    target_link_libraries(Pakal ${LUA_LIBRARIES})
endif()

# Platform-specific libraries
if(UNIX AND NOT APPLE)
    target_link_libraries(Pakal X11 GL pthread)
elseif(APPLE)
    find_package(OpenGL REQUIRED)
    target_link_libraries(Pakal OpenGL::OpenGL pthread)
elseif(WIN32)
    target_link_libraries(Pakal opengl32 user32 gdi32)
endif()

# Compiler flags for optimization
if(MSVC)
    target_compile_options(Pakal PRIVATE /O2 /W4)
else()
    target_compile_options(Pakal PRIVATE -O2 -Wall -Wextra 
        -Wno-address 
        -Wno-nonnull-compare
        -Wno-unused-parameter
        -Wno-ignored-qualifiers
        -Wno-implicit-fallthrough
        -Wno-deprecated-copy
        -Wno-strict-aliasing
        -Wno-maybe-uninitialized
        -Wno-delete-incomplete
        -Wno-missing-field-initializers
        -Wno-unused-variable)
    if(UNIX AND NOT APPLE)
        target_compile_options(Pakal PRIVATE -fPIC)
    endif()
endif()

# Add subdirectories for additional components if they have CMakeLists
if(EXISTS ${CMAKE_SOURCE_DIR}/source/components/CMakeLists.txt)
    add_subdirectory(${CMAKE_SOURCE_DIR}/source/components)
endif()

# Build examples
if(EXISTS ${CMAKE_SOURCE_DIR}/examples/basic_example/main.cpp)
    add_executable(PakalBasicExample ${CMAKE_SOURCE_DIR}/examples/basic_example/main.cpp)
    target_link_libraries(PakalBasicExample Pakal)
    target_include_directories(PakalBasicExample PRIVATE ${CMAKE_SOURCE_DIR}/source)
endif()

# Print configuration summary
message(STATUS "========== Pakal Engine Configuration ==========")
message(STATUS "C++ Standard: ${CMAKE_CXX_STANDARD}")
message(STATUS "Build Type: ${CMAKE_BUILD_TYPE}")
message(STATUS "SFML: ${SFML_FOUND}")
message(STATUS "Box2D: ${BOX2D_FOUND}")
message(STATUS "Lua: ${LUA_FOUND}")
message(STATUS "================================================")
