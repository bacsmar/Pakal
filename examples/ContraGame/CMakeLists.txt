# ContraGame CMakeLists.txt
project(ContraGame)

# Collect all source files
file(GLOB_RECURSE CONTRA_SOURCES
    "${CMAKE_CURRENT_SOURCE_DIR}/*.cpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/*.h"
)

set(CONTRA_HOST_MAIN ${CMAKE_CURRENT_SOURCE_DIR}/main.cpp)
set(CONTRA_MODULE_ENTRY ${CMAKE_CURRENT_SOURCE_DIR}/ContraGameModule.cpp)
set(CONTRA_MODULE_SOURCES ${CONTRA_SOURCES})
list(REMOVE_ITEM CONTRA_MODULE_SOURCES ${CONTRA_HOST_MAIN})

set(PAKAL_RUNTIME_DIR ${CMAKE_RUNTIME_OUTPUT_DIRECTORY})

# Create module
add_library(ContraGameModule SHARED ${CONTRA_MODULE_SOURCES})

# Link with Pakal engine
if(WIN32)
    target_link_libraries(ContraGameModule
        Pakal
    )
else()
    target_link_options(ContraGameModule PRIVATE -Wl,--allow-shlib-undefined)
endif()

# Include directories
target_include_directories(ContraGameModule PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}
    ${CMAKE_SOURCE_DIR}/source
    ${CMAKE_SOURCE_DIR}/source/bgfx
    ${CMAKE_SOURCE_DIR}/source/box2D
    ${CMAKE_SOURCE_DIR}/external/bgfx/include
    ${CMAKE_SOURCE_DIR}/external/bx/include
    ${CMAKE_SOURCE_DIR}/external/bimg/include
)

# Set C++ standard
set_property(TARGET ContraGameModule PROPERTY CXX_STANDARD 20)

# Copy assets to build directory
if(EXISTS ${CMAKE_CURRENT_SOURCE_DIR}/Assets)
    add_custom_command(TARGET ContraGameModule POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy_directory
        ${CMAKE_CURRENT_SOURCE_DIR}/Assets
        $<TARGET_FILE_DIR:ContraGameModule>/Assets
        COMMAND ${CMAKE_COMMAND} -E copy_directory
        ${CMAKE_CURRENT_SOURCE_DIR}/Assets
        ${PAKAL_RUNTIME_DIR}/Assets
    )
endif()

# Copy minimal bgfx shader binaries for sprite rendering
set(BGFX_SHADER_SRC_DIR ${CMAKE_SOURCE_DIR}/external/bgfx/examples/runtime/shaders)
add_custom_command(TARGET ContraGameModule POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E make_directory
        $<TARGET_FILE_DIR:ContraGameModule>/Assets/shaders/glsl
    COMMAND ${CMAKE_COMMAND} -E make_directory
        $<TARGET_FILE_DIR:ContraGameModule>/shaders/glsl
    COMMAND ${CMAKE_COMMAND} -E make_directory
        ${PAKAL_RUNTIME_DIR}/Assets/shaders/glsl
    COMMAND ${CMAKE_COMMAND} -E make_directory
        ${PAKAL_RUNTIME_DIR}/shaders/glsl
    COMMAND ${CMAKE_COMMAND} -E copy_if_different
        ${BGFX_SHADER_SRC_DIR}/glsl/vs_cubes.bin
        $<TARGET_FILE_DIR:ContraGameModule>/Assets/shaders/glsl/
    COMMAND ${CMAKE_COMMAND} -E copy_if_different
        ${BGFX_SHADER_SRC_DIR}/glsl/fs_cubes.bin
        $<TARGET_FILE_DIR:ContraGameModule>/Assets/shaders/glsl/
    COMMAND ${CMAKE_COMMAND} -E copy_if_different
        ${BGFX_SHADER_SRC_DIR}/glsl/vs_cubes.bin
        $<TARGET_FILE_DIR:ContraGameModule>/shaders/glsl/
    COMMAND ${CMAKE_COMMAND} -E copy_if_different
        ${BGFX_SHADER_SRC_DIR}/glsl/fs_cubes.bin
        $<TARGET_FILE_DIR:ContraGameModule>/shaders/glsl/
    COMMAND ${CMAKE_COMMAND} -E copy_if_different
        ${BGFX_SHADER_SRC_DIR}/glsl/vs_cubes.bin
        ${PAKAL_RUNTIME_DIR}/Assets/shaders/glsl/
    COMMAND ${CMAKE_COMMAND} -E copy_if_different
        ${BGFX_SHADER_SRC_DIR}/glsl/fs_cubes.bin
        ${PAKAL_RUNTIME_DIR}/Assets/shaders/glsl/
    COMMAND ${CMAKE_COMMAND} -E copy_if_different
        ${BGFX_SHADER_SRC_DIR}/glsl/vs_cubes.bin
        ${PAKAL_RUNTIME_DIR}/shaders/glsl/
    COMMAND ${CMAKE_COMMAND} -E copy_if_different
        ${BGFX_SHADER_SRC_DIR}/glsl/fs_cubes.bin
        ${PAKAL_RUNTIME_DIR}/shaders/glsl/
)

# Copy module near runtime host executable
add_custom_command(TARGET ContraGameModule POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy_if_different
        $<TARGET_FILE:ContraGameModule>
        ${PAKAL_RUNTIME_DIR}/
)

# Installation
install(TARGETS ContraGameModule DESTINATION bin)
