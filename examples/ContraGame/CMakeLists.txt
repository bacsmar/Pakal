# ContraGame CMakeLists.txt
# PURPOSE: Build ContraGame as a dynamic module (.so) for PakalPlayer
# ContraGame does NOT produce an executable - it's a plugin loaded by PakalPlayer core
#
# Architecture:
#   Pakal Core (source/) → PakalPlayer executable (loads any .so module)
#   ContraGame (examples/ContraGame/) → libContraGameModule.so (plugin only, no main)
#
# Usage:
#   PakalPlayer libContraGameModule.so

project(ContraGame)

# Collect all source files
file(GLOB_RECURSE CONTRA_SOURCES
    "${CMAKE_CURRENT_SOURCE_DIR}/*.cpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/*.h"
)
# Explicitly add the new Projectile component
list(APPEND CONTRA_SOURCES
    "${CMAKE_CURRENT_SOURCE_DIR}/Components/Projectile.cpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/Components/Projectile.h"
)

# Define entry points
set(CONTRA_HOST_MAIN ${CMAKE_CURRENT_SOURCE_DIR}/main.cpp)
set(CONTRA_MODULE_ENTRY ${CMAKE_CURRENT_SOURCE_DIR}/ContraGameModule.cpp)

# Module sources: Exclude standalone host main (only needed for self-contained executable)
# ContraGame is compiled ONLY as a dynamic module, not as a standalone executable
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
    # For dynamic loading, allow undefined symbols since they'll be resolved by PakalPlayer
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
        $<TARGET_FILE_DIR:ContraGameModule>/Assets/shaders/spirv
    COMMAND ${CMAKE_COMMAND} -E make_directory
        $<TARGET_FILE_DIR:ContraGameModule>/shaders/glsl
    COMMAND ${CMAKE_COMMAND} -E make_directory
        $<TARGET_FILE_DIR:ContraGameModule>/shaders/spirv
    COMMAND ${CMAKE_COMMAND} -E make_directory
        ${PAKAL_RUNTIME_DIR}/Assets/shaders/glsl
    COMMAND ${CMAKE_COMMAND} -E make_directory
        ${PAKAL_RUNTIME_DIR}/Assets/shaders/spirv
    COMMAND ${CMAKE_COMMAND} -E make_directory
        ${PAKAL_RUNTIME_DIR}/shaders/glsl
    COMMAND ${CMAKE_COMMAND} -E make_directory
        ${PAKAL_RUNTIME_DIR}/shaders/spirv
    COMMAND ${CMAKE_COMMAND} -E copy_if_different
        ${BGFX_SHADER_SRC_DIR}/glsl/vs_particle.bin
        $<TARGET_FILE_DIR:ContraGameModule>/Assets/shaders/glsl/
    COMMAND ${CMAKE_COMMAND} -E copy_if_different
        ${BGFX_SHADER_SRC_DIR}/glsl/fs_particle.bin
        $<TARGET_FILE_DIR:ContraGameModule>/Assets/shaders/glsl/
    COMMAND ${CMAKE_COMMAND} -E copy_if_different
        ${BGFX_SHADER_SRC_DIR}/glsl/vs_cubes.bin
        $<TARGET_FILE_DIR:ContraGameModule>/Assets/shaders/glsl/
    COMMAND ${CMAKE_COMMAND} -E copy_if_different
        ${BGFX_SHADER_SRC_DIR}/glsl/fs_cubes.bin
        $<TARGET_FILE_DIR:ContraGameModule>/Assets/shaders/glsl/
    COMMAND ${CMAKE_COMMAND} -E copy_if_different
        ${BGFX_SHADER_SRC_DIR}/glsl/vs_particle.bin
        $<TARGET_FILE_DIR:ContraGameModule>/shaders/glsl/
    COMMAND ${CMAKE_COMMAND} -E copy_if_different
        ${BGFX_SHADER_SRC_DIR}/glsl/fs_particle.bin
        $<TARGET_FILE_DIR:ContraGameModule>/shaders/glsl/
    COMMAND ${CMAKE_COMMAND} -E copy_if_different
        ${BGFX_SHADER_SRC_DIR}/glsl/vs_cubes.bin
        $<TARGET_FILE_DIR:ContraGameModule>/shaders/glsl/
    COMMAND ${CMAKE_COMMAND} -E copy_if_different
        ${BGFX_SHADER_SRC_DIR}/glsl/fs_cubes.bin
        $<TARGET_FILE_DIR:ContraGameModule>/shaders/glsl/
    COMMAND ${CMAKE_COMMAND} -E copy_if_different
        ${BGFX_SHADER_SRC_DIR}/glsl/vs_particle.bin
        ${PAKAL_RUNTIME_DIR}/Assets/shaders/glsl/
    COMMAND ${CMAKE_COMMAND} -E copy_if_different
        ${BGFX_SHADER_SRC_DIR}/glsl/fs_particle.bin
        ${PAKAL_RUNTIME_DIR}/Assets/shaders/glsl/
    COMMAND ${CMAKE_COMMAND} -E copy_if_different
        ${BGFX_SHADER_SRC_DIR}/glsl/vs_cubes.bin
        ${PAKAL_RUNTIME_DIR}/Assets/shaders/glsl/
    COMMAND ${CMAKE_COMMAND} -E copy_if_different
        ${BGFX_SHADER_SRC_DIR}/glsl/fs_cubes.bin
        ${PAKAL_RUNTIME_DIR}/Assets/shaders/glsl/
    COMMAND ${CMAKE_COMMAND} -E copy_if_different
        ${BGFX_SHADER_SRC_DIR}/glsl/vs_particle.bin
        ${PAKAL_RUNTIME_DIR}/shaders/glsl/
    COMMAND ${CMAKE_COMMAND} -E copy_if_different
        ${BGFX_SHADER_SRC_DIR}/glsl/fs_particle.bin
        ${PAKAL_RUNTIME_DIR}/shaders/glsl/
    COMMAND ${CMAKE_COMMAND} -E copy_if_different
        ${BGFX_SHADER_SRC_DIR}/glsl/vs_cubes.bin
        ${PAKAL_RUNTIME_DIR}/shaders/glsl/
    COMMAND ${CMAKE_COMMAND} -E copy_if_different
        ${BGFX_SHADER_SRC_DIR}/glsl/fs_cubes.bin
        ${PAKAL_RUNTIME_DIR}/shaders/glsl/
    COMMAND ${CMAKE_COMMAND} -E copy_if_different
        ${BGFX_SHADER_SRC_DIR}/spirv/vs_particle.bin
        $<TARGET_FILE_DIR:ContraGameModule>/Assets/shaders/spirv/
    COMMAND ${CMAKE_COMMAND} -E copy_if_different
        ${BGFX_SHADER_SRC_DIR}/spirv/fs_particle.bin
        $<TARGET_FILE_DIR:ContraGameModule>/Assets/shaders/spirv/
    COMMAND ${CMAKE_COMMAND} -E copy_if_different
        ${BGFX_SHADER_SRC_DIR}/spirv/vs_cubes.bin
        $<TARGET_FILE_DIR:ContraGameModule>/Assets/shaders/spirv/
    COMMAND ${CMAKE_COMMAND} -E copy_if_different
        ${BGFX_SHADER_SRC_DIR}/spirv/fs_cubes.bin
        $<TARGET_FILE_DIR:ContraGameModule>/Assets/shaders/spirv/
    COMMAND ${CMAKE_COMMAND} -E copy_if_different
        ${BGFX_SHADER_SRC_DIR}/spirv/vs_particle.bin
        $<TARGET_FILE_DIR:ContraGameModule>/shaders/spirv/
    COMMAND ${CMAKE_COMMAND} -E copy_if_different
        ${BGFX_SHADER_SRC_DIR}/spirv/fs_particle.bin
        $<TARGET_FILE_DIR:ContraGameModule>/shaders/spirv/
    COMMAND ${CMAKE_COMMAND} -E copy_if_different
        ${BGFX_SHADER_SRC_DIR}/spirv/vs_cubes.bin
        $<TARGET_FILE_DIR:ContraGameModule>/shaders/spirv/
    COMMAND ${CMAKE_COMMAND} -E copy_if_different
        ${BGFX_SHADER_SRC_DIR}/spirv/fs_cubes.bin
        $<TARGET_FILE_DIR:ContraGameModule>/shaders/spirv/
    COMMAND ${CMAKE_COMMAND} -E copy_if_different
        ${BGFX_SHADER_SRC_DIR}/spirv/vs_particle.bin
        ${PAKAL_RUNTIME_DIR}/Assets/shaders/spirv/
    COMMAND ${CMAKE_COMMAND} -E copy_if_different
        ${BGFX_SHADER_SRC_DIR}/spirv/fs_particle.bin
        ${PAKAL_RUNTIME_DIR}/Assets/shaders/spirv/
    COMMAND ${CMAKE_COMMAND} -E copy_if_different
        ${BGFX_SHADER_SRC_DIR}/spirv/vs_cubes.bin
        ${PAKAL_RUNTIME_DIR}/Assets/shaders/spirv/
    COMMAND ${CMAKE_COMMAND} -E copy_if_different
        ${BGFX_SHADER_SRC_DIR}/spirv/fs_cubes.bin
        ${PAKAL_RUNTIME_DIR}/Assets/shaders/spirv/
    COMMAND ${CMAKE_COMMAND} -E copy_if_different
        ${BGFX_SHADER_SRC_DIR}/spirv/vs_particle.bin
        ${PAKAL_RUNTIME_DIR}/shaders/spirv/
    COMMAND ${CMAKE_COMMAND} -E copy_if_different
        ${BGFX_SHADER_SRC_DIR}/spirv/fs_particle.bin
        ${PAKAL_RUNTIME_DIR}/shaders/spirv/
    COMMAND ${CMAKE_COMMAND} -E copy_if_different
        ${BGFX_SHADER_SRC_DIR}/spirv/vs_cubes.bin
        ${PAKAL_RUNTIME_DIR}/shaders/spirv/
    COMMAND ${CMAKE_COMMAND} -E copy_if_different
        ${BGFX_SHADER_SRC_DIR}/spirv/fs_cubes.bin
        ${PAKAL_RUNTIME_DIR}/shaders/spirv/
)

# Compile custom sprite shaders (RGBA sampling) with bgfx shaderc
set(SPRITE_SHADERC ${CMAKE_SOURCE_DIR}/external/bgfx/.build/linux64_gcc/bin/shadercRelease)
set(SPRITE_SHADER_SRC_DIR ${CMAKE_CURRENT_SOURCE_DIR}/shaders)
set(SPRITE_SHADER_INCLUDE_DIR ${CMAKE_SOURCE_DIR}/external/bgfx/src)

if(EXISTS ${SPRITE_SHADERC})
    add_custom_command(TARGET ContraGameModule POST_BUILD
        COMMAND ${SPRITE_SHADERC}
            -f ${SPRITE_SHADER_SRC_DIR}/vs_sprite.sc
            -o ${PAKAL_RUNTIME_DIR}/Assets/shaders/spirv/vs_sprite.bin
            --type vertex --platform linux --profile spirv
            --varyingdef ${SPRITE_SHADER_SRC_DIR}/varying.def.sc
            -i ${SPRITE_SHADER_INCLUDE_DIR}
        COMMAND ${SPRITE_SHADERC}
            -f ${SPRITE_SHADER_SRC_DIR}/fs_sprite.sc
            -o ${PAKAL_RUNTIME_DIR}/Assets/shaders/spirv/fs_sprite.bin
            --type fragment --platform linux --profile spirv
            --varyingdef ${SPRITE_SHADER_SRC_DIR}/varying.def.sc
            -i ${SPRITE_SHADER_INCLUDE_DIR}
        COMMAND ${SPRITE_SHADERC}
            -f ${SPRITE_SHADER_SRC_DIR}/vs_sprite.sc
            -o ${PAKAL_RUNTIME_DIR}/Assets/shaders/glsl/vs_sprite.bin
            --type vertex --platform linux --profile 120
            --varyingdef ${SPRITE_SHADER_SRC_DIR}/varying.def.sc
            -i ${SPRITE_SHADER_INCLUDE_DIR}
        COMMAND ${SPRITE_SHADERC}
            -f ${SPRITE_SHADER_SRC_DIR}/fs_sprite.sc
            -o ${PAKAL_RUNTIME_DIR}/Assets/shaders/glsl/fs_sprite.bin
            --type fragment --platform linux --profile 120
            --varyingdef ${SPRITE_SHADER_SRC_DIR}/varying.def.sc
            -i ${SPRITE_SHADER_INCLUDE_DIR}
        COMMAND ${CMAKE_COMMAND} -E copy_if_different
            ${PAKAL_RUNTIME_DIR}/Assets/shaders/spirv/vs_sprite.bin
            $<TARGET_FILE_DIR:ContraGameModule>/Assets/shaders/spirv/
        COMMAND ${CMAKE_COMMAND} -E copy_if_different
            ${PAKAL_RUNTIME_DIR}/Assets/shaders/spirv/fs_sprite.bin
            $<TARGET_FILE_DIR:ContraGameModule>/Assets/shaders/spirv/
        COMMAND ${CMAKE_COMMAND} -E copy_if_different
            ${PAKAL_RUNTIME_DIR}/Assets/shaders/glsl/vs_sprite.bin
            $<TARGET_FILE_DIR:ContraGameModule>/Assets/shaders/glsl/
        COMMAND ${CMAKE_COMMAND} -E copy_if_different
            ${PAKAL_RUNTIME_DIR}/Assets/shaders/glsl/fs_sprite.bin
            $<TARGET_FILE_DIR:ContraGameModule>/Assets/shaders/glsl/
    )
else()
    message(WARNING "sprite shaderc not found at ${SPRITE_SHADERC}; using fallback shaders")
endif()

# Copy module near runtime host executable
add_custom_command(TARGET ContraGameModule POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy_if_different
        $<TARGET_FILE:ContraGameModule>
        ${PAKAL_RUNTIME_DIR}/
)

# Installation
install(TARGETS ContraGameModule DESTINATION bin)
