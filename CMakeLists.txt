cmake_minimum_required(VERSION 3.10)
# Use CMAKE_MSVC_RUNTIME_LIBRARY (needed for the runtime selection below).
if(POLICY CMP0091)
    cmake_policy(SET CMP0091 NEW)
endif()

project(Pakal)
set(CMAKE_POSITION_INDEPENDENT_CODE ON)

# Dynamic CRT on MSVC: bgfx is built with /MD (genie --with-dynamic-runtime) and
# the vcpkg packages use the x64-windows-static-md triplet (see CMakePresets.json).
if(MSVC)
    set(CMAKE_MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>DLL")
endif()

# Set C++ standard
set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# bx's SIMD code (its headers, and bgfx's examples/common the engine compiles) uses SSE 4.1 intrinsics: GCC and Clang
# refuse them without the target, as bx's own build does (-msse4.2). MSVC needs no switch; Android is ARM.
if(NOT MSVC AND NOT ANDROID AND CMAKE_SYSTEM_PROCESSOR MATCHES "x86_64|AMD64|amd64")
    add_compile_options(-msse4.2)
endif()

# Set output directories
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin)
set(CMAKE_LIBRARY_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/lib)
set(CMAKE_ARCHIVE_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/lib)

# Tests run with ctest (ctest --test-dir build-win64 --output-on-failure)
enable_testing()

# Create a config.h compatibility header for case-insensitive systems.
# Not on Windows: there it would resolve to itself, since the filesystem ignores case.
if(NOT WIN32)
    file(GENERATE OUTPUT "${CMAKE_BINARY_DIR}/config.h" CONTENT "#include \"Config.h\"\n")
endif()

# The engine's core alone (no backends, the dummy systems), built with every build so that the core stays apart from
# them; PAKAL_BUILD_CORE_ONLY builds it and nothing else (no bgfx, SFML, Box2D, Assimp or RmlUi needed)
option(PAKAL_BUILD_CORE_ONLY "Build only the engine's core (PakalCore and CoreTests)" OFF)
# Files reloaded while the game runs (.rml, .rcss, texts, data: docs/UI_RELOAD.md): on in the desktop's builds; OFF for a
# build that goes to players (Android has it only with build_apk.py --tools)
option(PAKAL_FILE_RELOAD "Reload UI and data files while the game runs (dev builds)" ON)
if(NOT PAKAL_FILE_RELOAD)
    add_compile_definitions(PAKAL_NO_FILE_RELOAD)
endif()
include(${CMAKE_SOURCE_DIR}/cmake/PakalCore.cmake)

# Games and genres never see the backends: every build checks that no game or genre includes a backend header beyond
# the files still waiting to move to the abstractions (cmake/backend_includes_pending.txt, a list that only shrinks)
set(PAKAL_CHECK_BACKEND_INCLUDES ${CMAKE_COMMAND} -DPAKAL_ROOT=${CMAKE_SOURCE_DIR} -P ${CMAKE_SOURCE_DIR}/cmake/CheckBackendIncludes.cmake)
add_custom_target(CheckBackendIncludes ALL COMMAND ${PAKAL_CHECK_BACKEND_INCLUDES} VERBATIM)
add_test(NAME BackendIncludes COMMAND ${PAKAL_CHECK_BACKEND_INCLUDES})
# The C-DAT scan (tools/cdat_scan.py: UI built or styled in a game's code, docs/UI_RELOAD.md), its own tests
find_package(Python3 COMPONENTS Interpreter QUIET)
if(Python3_Interpreter_FOUND)
    add_test(NAME CdatScanTests COMMAND ${Python3_EXECUTABLE} -m unittest tests/test_cdat_scan.py WORKING_DIRECTORY ${CMAKE_SOURCE_DIR})
    # a game pack's key on no command line (build_apk.py seals in its own process; pakal_pack.py reads it from the environment)
    add_test(NAME PackKeyTests COMMAND ${Python3_EXECUTABLE} -m unittest tests/test_pack_key.py WORKING_DIRECTORY ${CMAKE_SOURCE_DIR})
endif()
# how a game builds on the engine: the engine's libraries by name (Pakal::TopDownView...) and pakal_add_game_module()
include(${CMAKE_SOURCE_DIR}/cmake/PakalGameModule.cmake)
if(PAKAL_BUILD_CORE_ONLY)
    return()
endif()

# Our fixes to bgfx: applied to the submodule here; cmake/BgfxBuild.cmake builds the desktop's libraries again with them
set(PAKAL_ROOT ${CMAKE_SOURCE_DIR})
include(${CMAKE_SOURCE_DIR}/cmake/BgfxPatches.cmake)

# Include directories
include_directories(${CMAKE_BINARY_DIR})
include_directories(${CMAKE_SOURCE_DIR}/source)
include_directories(${CMAKE_SOURCE_DIR}/external/bx/include)
include_directories(${CMAKE_SOURCE_DIR}/external/bgfx/include)
include_directories(${CMAKE_SOURCE_DIR}/external/bimg/include)
include_directories(${CMAKE_SOURCE_DIR}/external/bgfx/examples/common)

# Add source files
file(GLOB_RECURSE PAKAL_SOURCES FOLLOW_SYMLINKS
    "${CMAKE_SOURCE_DIR}/source/*.cpp"
    "${CMAKE_SOURCE_DIR}/source/*.h"
    "${CMAKE_SOURCE_DIR}/source/components/*.cpp"
    "${CMAKE_SOURCE_DIR}/source/components/*.h"
    "${CMAKE_SOURCE_DIR}/source/resources/*.cpp"
    "${CMAKE_SOURCE_DIR}/source/resources/*.h"
)

# poly2tri (used by DynamicMesh) uses .cc sources
file(GLOB_RECURSE PAKAL_POLY2TRI_SOURCES
    "${CMAKE_SOURCE_DIR}/source/components/DynamicMesh/poly2tri/*.cc"
)
list(APPEND PAKAL_SOURCES ${PAKAL_POLY2TRI_SOURCES})

# Exclude platform-specific files
list(FILTER PAKAL_SOURCES EXCLUDE REGEX ".*/android/.*")
list(FILTER PAKAL_SOURCES EXCLUDE REGEX ".*/win32/.*")
list(FILTER PAKAL_SOURCES EXCLUDE REGEX ".*/unix/.*")
list(FILTER PAKAL_SOURCES EXCLUDE REGEX ".*/examples/.*")
# The TopDown module is a library of its own (PakalTopDown, below), not part of the engine
# The scripts' host is a library of its own too (PakalScripting): the engine does not depend on Lua
list(FILTER PAKAL_SOURCES EXCLUDE REGEX ".*/source/scripting/.*")
# the network (ENet) is a library of its own: only what plays over it links it
list(FILTER PAKAL_SOURCES EXCLUDE REGEX ".*/source/net/.*")
# pugixml is compiled once, through the unity unit source/CompileUnit_1.cpp
list(FILTER PAKAL_SOURCES EXCLUDE REGEX ".*/persist/pugixml/pugixml\\.cpp$")
# the core alone's default systems (the dummies) are PakalCore's; this engine's are in source/runtime
list(FILTER PAKAL_SOURCES EXCLUDE REGEX ".*/source/core_only/.*")

# Platform-specific source files
if(UNIX AND NOT APPLE)
    list(APPEND PAKAL_SOURCES ${CMAKE_SOURCE_DIR}/source/unix/ClockImpl.cpp ${CMAKE_SOURCE_DIR}/source/unix/HttpPostImpl.cpp)
elseif(APPLE)
    list(APPEND PAKAL_SOURCES ${CMAKE_SOURCE_DIR}/source/unix/ClockImpl.cpp ${CMAKE_SOURCE_DIR}/source/unix/HttpPostImpl.cpp)
elseif(WIN32)
    list(APPEND PAKAL_SOURCES ${CMAKE_SOURCE_DIR}/source/win32/ClockImpl.cpp ${CMAKE_SOURCE_DIR}/source/win32/HttpPostImpl.cpp)
endif()

# Create the main library.
# Desktop game modules and their player share one instance of the engine singletons.
if(WIN32)
    add_library(Pakal SHARED ${PAKAL_SOURCES})
    set_target_properties(Pakal PROPERTIES WINDOWS_EXPORT_ALL_SYMBOLS ON)
    # Building the engine: export the PAKAL_API_* symbols; consumers import them.
    target_compile_definitions(Pakal PRIVATE PAKAL_NONCLIENT_BUILD)
elseif(UNIX AND NOT APPLE)
    add_library(Pakal SHARED ${PAKAL_SOURCES})
else()
    add_library(Pakal STATIC ${PAKAL_SOURCES})
endif()
# the check runs with any build of something on the engine (a game module alone too), not only a full build
add_dependencies(Pakal CheckBackendIncludes)
# build.json beside the programs at every build: the copy's commit and channel, which the engine logs and draws as its label
# (docs/BUILD_IDENTITY.md)
add_custom_target(PakalBuildIdentity
    COMMAND ${CMAKE_COMMAND} -DROOT=${CMAKE_SOURCE_DIR} -DOUT=${CMAKE_RUNTIME_OUTPUT_DIRECTORY}/build.json
        -P ${CMAKE_SOURCE_DIR}/cmake/WriteBuildIdentity.cmake
    BYPRODUCTS ${CMAKE_RUNTIME_OUTPUT_DIRECTORY}/build.json
    COMMENT "build.json (the build's identity)" VERBATIM)
add_dependencies(Pakal PakalBuildIdentity)
# The genres (game_genres/: TopDown) live outside the engine's source/; what is built after the engine finds them as
# "topdown/..."
include_directories(${CMAKE_SOURCE_DIR}/game_genres)

# bgfx debugdraw implementation (used by Box2D debug rendering with bgfx)
target_sources(Pakal PRIVATE
    ${CMAKE_SOURCE_DIR}/external/bgfx/examples/common/debugdraw/debugdraw.cpp
)
# Dear ImGui (bundled with bgfx) and bgfx's renderer for it: the development panels (source/bgfx/BgfxDebugUI)
set(PAKAL_IMGUI_SOURCES
    ${CMAKE_SOURCE_DIR}/external/bgfx/3rdparty/dear-imgui/imgui.cpp
    ${CMAKE_SOURCE_DIR}/external/bgfx/3rdparty/dear-imgui/imgui_draw.cpp
    ${CMAKE_SOURCE_DIR}/external/bgfx/3rdparty/dear-imgui/imgui_tables.cpp
    ${CMAKE_SOURCE_DIR}/external/bgfx/3rdparty/dear-imgui/imgui_widgets.cpp
    ${CMAKE_SOURCE_DIR}/external/bgfx/examples/common/imgui/imgui.cpp)
target_sources(Pakal PRIVATE ${PAKAL_IMGUI_SOURCES})
if(MSVC)
    # bgfx's ImGui widgets list folders with dirent.h, which bx provides for MSVC
    set_source_files_properties(${PAKAL_IMGUI_SOURCES} PROPERTIES INCLUDE_DIRECTORIES ${CMAKE_SOURCE_DIR}/external/bx/include/compat/msvc)
endif()
target_include_directories(Pakal PUBLIC ${CMAKE_SOURCE_DIR}/external/bgfx/3rdparty)
# meshoptimizer (bundled with bgfx): simplified shadow casters
file(GLOB PAKAL_MESHOPT_SOURCES ${CMAKE_SOURCE_DIR}/external/bgfx/3rdparty/meshoptimizer/src/*.cpp)
target_sources(Pakal PRIVATE ${PAKAL_MESHOPT_SOURCES})
target_include_directories(Pakal PRIVATE ${CMAKE_SOURCE_DIR}/external/bgfx/3rdparty/meshoptimizer/src)
# miniz for ResourceManager's zip source (source/bgfx/ZipSourceMiniz.cpp), with bimg's header. bimg_decode no longer
# carries it (built without EXR: that miniz clashed with Assimp's). Where vcpkg's Assimp brings kubazip, which has miniz
# inside, its functions are used (as on Android); with a system Assimp (Linux), bimg's miniz.c is compiled in
find_package(kubazip CONFIG QUIET)
if(TARGET kubazip::kubazip)
    set(PAKAL_MINIZ_LIBS kubazip::kubazip)
    set(PAKAL_MINIZ_SOURCES "")
else()
    set(PAKAL_MINIZ_LIBS "")
    set(PAKAL_MINIZ_SOURCES ${CMAKE_SOURCE_DIR}/external/bimg/3rdparty/tinyexr/deps/miniz/miniz.c)
endif()
target_include_directories(Pakal PRIVATE ${CMAKE_SOURCE_DIR}/external/bimg/3rdparty/tinyexr/deps/miniz)
target_sources(Pakal PRIVATE ${PAKAL_MINIZ_SOURCES})
target_link_libraries(Pakal PRIVATE ${PAKAL_MINIZ_LIBS})

# Set include paths for the library
target_include_directories(Pakal PUBLIC
    ${CMAKE_BINARY_DIR}
    ${CMAKE_SOURCE_DIR}/source
    ${CMAKE_SOURCE_DIR}/external/bx/include
    ${CMAKE_SOURCE_DIR}/external/bgfx/include
    ${CMAKE_SOURCE_DIR}/external/bimg/include
    ${CMAKE_SOURCE_DIR}/external/bgfx/examples/common
)

# Dependencies are PRIVATE: consumers (PakalPlayer, game modules) link only Pakal itself.

# MCP server (source/mcp): Winsock for the socket, nlohmann-json for the JSON-RPC messages
find_package(nlohmann_json CONFIG REQUIRED)
target_link_libraries(Pakal PRIVATE nlohmann_json::nlohmann_json)
if(WIN32)
    target_link_libraries(Pakal PRIVATE ws2_32 winmm psapi)
endif()

# Engine's bundled pugixml (1.4, in the persist submodule) lives in the same namespace as the
# pugixml 1.16 that Assimp links. Rename the engine copy so both can be linked into Pakal.dll.
# Remove this once source/persist is updated to a pugixml that matches vcpkg's.
target_compile_definitions(Pakal PRIVATE pugi=pugi_pakal p2t=p2t_pakal)

# stb_vorbis (vcpkg's stb; Linux: libstb-dev's stb_vorbis.h in /usr/include/stb, the same file): the music's OGG decoder
# (source/MusicMixer.cpp; without it the music is silent)
find_path(PAKAL_STB_DIR NAMES stb_vorbis.c stb_vorbis.h PATH_SUFFIXES stb)
if(PAKAL_STB_DIR)
    target_include_directories(Pakal PRIVATE ${PAKAL_STB_DIR})
else()
    message(WARNING "stb_vorbis.c not found (vcpkg's stb): the game plays without music")
endif()

# SFML
if(WIN32)
    # vcpkg provides SFML 2.6. The code uses the SFML 2 event API (pollEvent + sf::Event::Closed),
    # which SFML 3 removed. Static SFML also pulls in its dependencies through its config.
    set(SFML_STATIC_LIBRARIES TRUE)
    find_package(SFML 2.6 COMPONENTS system window audio REQUIRED)
    target_link_libraries(Pakal PRIVATE sfml-system sfml-window sfml-audio)
    # PRIVATE: only the engine's desktop backend (source/sfml, included by source/runtime/EngineBackends.cpp) sees SFML;
    # games reach sound, input and the window through the abstractions (docs/adr ADR-0017, ADR-0018 step 0).
    get_target_property(PAKAL_SFML_INCLUDES sfml-audio INTERFACE_INCLUDE_DIRECTORIES)
    target_include_directories(Pakal PRIVATE ${PAKAL_SFML_INCLUDES})
    target_compile_definitions(Pakal PRIVATE SFML_STATIC)
    # OpenAL (static, from vcpkg) needs fmt and the MMCSS audio thread API
    find_package(fmt CONFIG REQUIRED)
    target_link_libraries(Pakal PRIVATE fmt::fmt avrt)
    message(STATUS "✓ SFML found via vcpkg")
else()
    find_package(PkgConfig REQUIRED)
    pkg_check_modules(SFML REQUIRED sfml-system sfml-window sfml-audio)

    if(SFML_FOUND)
        message(STATUS "✓ SFML found via pkg-config")
        target_include_directories(Pakal PRIVATE ${SFML_INCLUDE_DIRS})
        target_link_directories(Pakal PRIVATE ${SFML_LIBRARY_DIRS})
        target_link_libraries(Pakal PRIVATE ${SFML_LIBRARIES})
    else()
        message(FATAL_ERROR "❌ SFML not found. Install with: sudo apt-get install libsfml-dev")
    endif()
endif()

# bgfx library (prebuilt from the submodule's own build tools; we link the libraries)
set(BGFX_DIR ${CMAKE_SOURCE_DIR}/external/bgfx)
# Built from the submodules the first time CMake configures (cmake/BgfxBuild.cmake), our patches applied
include(${CMAKE_SOURCE_DIR}/cmake/BgfxBuild.cmake)

if(WIN32)
    # Built with: genie vs2022 --with-dynamic-runtime (external/bgfx), then MSBuild
    # with PlatformToolset v145, Release|x64. See docs/BUILDING.md.
    # bgfx's C++ API is not exported by the shared build, so it is linked statically
    # into Pakal.dll only. Game modules do not call bgfx directly.
    set(BGFX_BUILD_DIR ${BGFX_DIR}/.build/win64_vs2022/bin)
    set(BGFX_LIBS
        ${BGFX_BUILD_DIR}/bgfxRelease.lib
        ${BGFX_BUILD_DIR}/bxRelease.lib
        ${BGFX_BUILD_DIR}/bimg_decodeRelease.lib
        ${BGFX_BUILD_DIR}/bimgRelease.lib
    )
    set(BGFX_SYSTEM_LIBS "")
else()
    # Built by cmake/BgfxBuild.cmake (make linux-gcc-release64)
    set(BGFX_BUILD_DIR ${BGFX_DIR}/.build/linux64_gcc/bin)
    set(BGFX_LIBS
        ${BGFX_BUILD_DIR}/libbgfxRelease.a
        ${BGFX_BUILD_DIR}/libbxRelease.a
        ${BGFX_BUILD_DIR}/libbimg_decodeRelease.a
        ${BGFX_BUILD_DIR}/libbimgRelease.a
    )
    set(BGFX_SYSTEM_LIBS dl)
endif()

list(GET BGFX_LIBS 0 BGFX_MAIN_LIB)
if(EXISTS ${BGFX_MAIN_LIB})
    message(STATUS "✓ Found prebuilt bgfx library")
    target_link_libraries(Pakal PRIVATE
        ${BGFX_LIBS}
        ${BGFX_SYSTEM_LIBS}
    )
else()
    message(WARNING "⚠️  bgfx library not found in ${BGFX_BUILD_DIR}")
endif()

# Engine shaders: Direct3D 11 on Windows; OpenGL and Vulkan on Linux.
if(WIN32 OR (UNIX AND NOT APPLE))
    if(WIN32)
        set(PAKAL_SHADERC ${BGFX_DIR}/.build/win64_vs2022/bin/shadercRelease.exe)
        set(PAKAL_SHADER_PLATFORM windows)
        set(PAKAL_SHADER_PROFILE s_5_0)
        set(PAKAL_SHADER_BACKEND dx11)
    else()
        set(PAKAL_SHADERC ${BGFX_BUILD_DIR}/shadercRelease)
        set(PAKAL_SHADER_PLATFORM linux)
        set(PAKAL_SHADER_PROFILE 430)
        set(PAKAL_SHADER_BACKEND glsl)
    endif()
    set(PAKAL_SHADER_SRC ${CMAKE_SOURCE_DIR}/source/bgfx/shaders)
    set(PAKAL_SHADER_DIR ${CMAKE_RUNTIME_OUTPUT_DIRECTORY}/shaders/${PAKAL_SHADER_BACKEND})
    if(EXISTS ${PAKAL_SHADERC})
        function(pakal_compile_shader name type varying)
            set(backends ${PAKAL_SHADER_BACKEND})
            if(UNIX AND NOT APPLE)
                list(APPEND backends spirv)
            endif()
            foreach(backend IN LISTS backends)
                set(shader_profile ${PAKAL_SHADER_PROFILE})
                if(backend STREQUAL "spirv")
                    set(shader_profile spirv)
                endif()
                set(shader_dir ${CMAKE_RUNTIME_OUTPUT_DIRECTORY}/shaders/${backend})
                set(out ${shader_dir}/${name}.bin)
                add_custom_command(OUTPUT ${out}
                    COMMAND ${CMAKE_COMMAND} -E make_directory ${shader_dir}
                    COMMAND ${PAKAL_SHADERC}
                        -f ${PAKAL_SHADER_SRC}/${name}.sc
                        -o ${out}
                        --type ${type} --platform ${PAKAL_SHADER_PLATFORM} -p ${shader_profile}
                        --varyingdef ${PAKAL_SHADER_SRC}/${varying}
                        -i ${BGFX_DIR}/src
                        -i ${PAKAL_SHADER_SRC}
                    DEPENDS ${PAKAL_SHADER_SRC}/${name}.sc ${PAKAL_SHADER_SRC}/${varying} ${PAKAL_SHADER_SRC}/pakal_lighting.sh ${PAKAL_SHADER_SRC}/pakal_precision.sh ${PAKAL_SHADER_SRC}/pakal_cutout.sh ${PAKAL_SHADER_SRC}/pakal_wind.sh ${PAKAL_SHADER_SRC}/pakal_ao_depth.sh
                    VERBATIM)
                list(APPEND PAKAL_SHADER_OUTPUTS ${out})
            endforeach()
            set(PAKAL_SHADER_OUTPUTS ${PAKAL_SHADER_OUTPUTS} PARENT_SCOPE)
        endfunction()

        pakal_compile_shader(vs_mesh vertex varying_mesh.def.sc)
        pakal_compile_shader(vs_skin vertex varying_skin.def.sc)
        pakal_compile_shader(vs_skin_pbr vertex varying_skin_pbr.def.sc)
        pakal_compile_shader(vs_pbr vertex varying_pbr.def.sc)
        pakal_compile_shader(fs_pbr fragment varying_pbr.def.sc)
        pakal_compile_shader(fs_mesh fragment varying_mesh.def.sc)
        pakal_compile_shader(vs_sky vertex varying_sky.def.sc)
        pakal_compile_shader(fs_sky fragment varying_sky.def.sc)
        pakal_compile_shader(vs_shadow vertex varying_shadow.def.sc)
        pakal_compile_shader(vs_pbr_inst vertex varying_pbr.def.sc)
        pakal_compile_shader(vs_mesh_inst vertex varying_mesh.def.sc)
        pakal_compile_shader(vs_shadow_inst vertex varying_shadow.def.sc)
        pakal_compile_shader(vs_shadow_cutout vertex varying_shadow_cutout.def.sc)
        pakal_compile_shader(fs_shadow_cutout fragment varying_shadow_cutout.def.sc)
        pakal_compile_shader(vs_grass vertex varying_grass.def.sc)
        pakal_compile_shader(fs_grass fragment varying_grass.def.sc)
        pakal_compile_shader(vs_gpu_particle vertex varying_particle.def.sc)
        pakal_compile_shader(cs_particles compute varying_particle.def.sc)
        pakal_compile_shader(vs_shadow_skin vertex varying_shadow.def.sc)
        pakal_compile_shader(fs_shadow fragment varying_shadow.def.sc)
        pakal_compile_shader(vs_post vertex varying_post.def.sc)
        pakal_compile_shader(fs_post fragment varying_post.def.sc)
        pakal_compile_shader(fs_fsr_easu fragment varying_post.def.sc)
        pakal_compile_shader(fs_fsr_rcas fragment varying_post.def.sc)
        pakal_compile_shader(fs_bloom_down fragment varying_post.def.sc)
        pakal_compile_shader(fs_bloom_blur fragment varying_post.def.sc)
        pakal_compile_shader(fs_ao fragment varying_post.def.sc)
        pakal_compile_shader(fs_ao_blur fragment varying_post.def.sc)
        pakal_compile_shader(vs_particle vertex varying_particle.def.sc)
        pakal_compile_shader(fs_particle fragment varying_particle.def.sc)
        pakal_compile_shader(fs_particle_sprite fragment varying_particle.def.sc)
        pakal_compile_shader(vs_sprite_simple vertex varying_sprite.def.sc)
        pakal_compile_shader(fs_sprite_simple fragment varying_sprite.def.sc)
        pakal_compile_shader(vs_ui vertex varying_ui.def.sc)
        pakal_compile_shader(fs_ui fragment varying_ui.def.sc)

        add_custom_target(PakalShaders ALL DEPENDS ${PAKAL_SHADER_OUTPUTS})
        add_dependencies(Pakal PakalShaders)
    else()
        message(WARNING "Shader compiler not found at ${PAKAL_SHADERC}; 3D meshes will not render")
    endif()
endif()

# Assimp: model loading for MeshComponent3D_Bgfx (required).
# Its headers appear in the public MeshComponent3D_Bgfx.h, so the include paths are PUBLIC.
# The library itself stays PRIVATE: linking it into consumers would duplicate it.
if(WIN32)
    find_package(assimp CONFIG REQUIRED)
    target_link_libraries(Pakal PRIVATE assimp::assimp)
    get_target_property(PAKAL_ASSIMP_INCLUDES assimp::assimp INTERFACE_INCLUDE_DIRECTORIES)
    target_include_directories(Pakal PUBLIC ${PAKAL_ASSIMP_INCLUDES})
else()
    pkg_check_modules(ASSIMP REQUIRED assimp)
    target_include_directories(Pakal PUBLIC ${ASSIMP_INCLUDE_DIRS})
    target_link_libraries(Pakal PRIVATE ${ASSIMP_LIBRARIES})
endif()

# RmlUi: the game's interface (source/bgfx/BgfxRmlUi). Private: games use it through IUIManager.
find_package(RmlUi CONFIG REQUIRED)
target_link_libraries(Pakal PRIVATE RmlUi::RmlUi)

# Box2D physics library. PRIVATE: no public header of the 2D physics names it (docs/adr ADR-0017,
# phase C), only the engine's .cpp files (source/box2D/Box2DInternal.h).
if(WIN32)
    find_package(box2d CONFIG REQUIRED)
    target_link_libraries(Pakal PRIVATE box2d::box2d)
    get_target_property(PAKAL_BOX2D_INCLUDES box2d::box2d INTERFACE_INCLUDE_DIRECTORIES)
    target_include_directories(Pakal PRIVATE ${PAKAL_BOX2D_INCLUDES})
    set(BOX2D_FOUND TRUE)
    message(STATUS "✓ Box2D found via vcpkg")
else()
    pkg_check_modules(BOX2D box2d)
    if(BOX2D_FOUND)
        message(STATUS "✓ Box2D found via pkg-config")
        target_include_directories(Pakal PRIVATE ${BOX2D_INCLUDE_DIRS})
        target_link_directories(Pakal PRIVATE ${BOX2D_LIBRARY_DIRS})
        target_link_libraries(Pakal PRIVATE ${BOX2D_LIBRARIES})
    else()
        message(STATUS "⚠️  Box2D not found. Install with: sudo apt-get install libbox2d-dev")
    endif()
endif()

# Lua (optional, for scripting)
find_package(Lua 5.4 QUIET)
if(LUA_FOUND)
    target_include_directories(Pakal PRIVATE ${LUA_INCLUDE_DIR})
    target_link_libraries(Pakal PRIVATE ${LUA_LIBRARIES})
    if(NOT WIN32)
        add_library(PakalScripting STATIC ${CMAKE_SOURCE_DIR}/source/scripting/ScriptHost.cpp)
        target_include_directories(PakalScripting PUBLIC ${CMAKE_SOURCE_DIR}/source ${LUA_INCLUDE_DIR})
        target_link_libraries(PakalScripting PUBLIC Pakal ${LUA_LIBRARIES})
    endif()
endif()

# Platform-specific libraries
if(UNIX AND NOT APPLE)
    target_link_libraries(Pakal PRIVATE X11 GL pthread)
elseif(APPLE)
    find_package(OpenGL REQUIRED)
    target_link_libraries(Pakal PRIVATE OpenGL::OpenGL pthread)
elseif(WIN32)
    target_link_libraries(Pakal PRIVATE opengl32 user32 gdi32)
endif()

# Compiler flags for optimization
if(MSVC)
    target_compile_options(Pakal PRIVATE /W4)
    # bx requires the standard-conforming preprocessor and __cplusplus value on MSVC
    target_compile_options(Pakal PUBLIC /Zc:__cplusplus /Zc:preprocessor)
    target_compile_options(Pakal PRIVATE
        $<$<CONFIG:Debug>:/Od /Zi>
        $<$<CONFIG:RelWithDebInfo>:/O2 /Zi>
        $<$<CONFIG:Release>:/O2>
        $<$<CONFIG:MinSizeRel>:/O1>)
    target_compile_definitions(Pakal PUBLIC
        $<$<CONFIG:Debug>:_DEBUG>
        $<$<NOT:$<CONFIG:Debug>>:NDEBUG>
        $<$<CONFIG:Debug>:BX_CONFIG_DEBUG=1>
        $<$<NOT:$<CONFIG:Debug>>:BX_CONFIG_DEBUG=0>)
else()
    target_compile_options(Pakal PRIVATE -Wall -Wextra
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
    target_compile_options(Pakal PRIVATE
        $<$<CONFIG:Debug>:-O0 -g>
        $<$<CONFIG:RelWithDebInfo>:-O2 -g>
        $<$<CONFIG:Release>:-O3>
        $<$<CONFIG:MinSizeRel>:-Os>)
    if(UNIX AND NOT APPLE)
        target_compile_options(Pakal PRIVATE -fPIC)
    endif()
    # Define debug/release macros and BX debug flag per config
    target_compile_definitions(Pakal PUBLIC
        $<$<CONFIG:Debug>:_DEBUG>
        $<$<NOT:$<CONFIG:Debug>>:NDEBUG>
        $<$<CONFIG:Debug>:BX_CONFIG_DEBUG=1>
        $<$<NOT:$<CONFIG:Debug>>:BX_CONFIG_DEBUG=0>)
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

# The engine's own logo, which SplashState shows when a game names none (docs/SPLASH.md), next to every build
add_custom_command(TARGET Pakal POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy_if_different ${CMAKE_SOURCE_DIR}/assets/engine/pakal_splash.png
        ${CMAKE_RUNTIME_OUTPUT_DIRECTORY}/Assets/pakal/splash.png
    VERBATIM)

if(EXISTS ${CMAKE_SOURCE_DIR}/examples/engine_feature_lab/main.cpp)
    add_executable(PakalFeatureLab ${CMAKE_SOURCE_DIR}/examples/engine_feature_lab/main.cpp)
    target_link_libraries(PakalFeatureLab Pakal)
    target_include_directories(PakalFeatureLab PRIVATE ${CMAKE_SOURCE_DIR}/source)
endif()

# Build generic runtime host for game modules
if(EXISTS ${CMAKE_SOURCE_DIR}/apps/player/PakalPlayerMain.cpp)
    add_executable(PakalPlayer
        ${CMAKE_SOURCE_DIR}/apps/player/PakalPlayerMain.cpp
        ${CMAKE_SOURCE_DIR}/source/resources/SceneLoader.cpp
    )
    target_link_libraries(PakalPlayer Pakal)
    target_include_directories(PakalPlayer PRIVATE ${CMAKE_SOURCE_DIR}/source)
    if(UNIX AND NOT APPLE)
        target_link_options(PakalPlayer PRIVATE -Wl,-E)
    endif()
    set_target_properties(PakalPlayer PROPERTIES ENABLE_EXPORTS ON)
    # The same player without a console window, for a game handed out (renamed to the game, beside its pakal_module.txt):
    # a Windows program (no console) that still starts at main
    if(WIN32)
        add_executable(PakalPlayerWindowed WIN32
            ${CMAKE_SOURCE_DIR}/apps/player/PakalPlayerMain.cpp
            ${CMAKE_SOURCE_DIR}/source/resources/SceneLoader.cpp
        )
        target_link_libraries(PakalPlayerWindowed Pakal)
        target_include_directories(PakalPlayerWindowed PRIVATE ${CMAKE_SOURCE_DIR}/source)
        set_target_properties(PakalPlayerWindowed PROPERTIES ENABLE_EXPORTS ON)
        if(MSVC)
            target_link_options(PakalPlayerWindowed PRIVATE /ENTRY:mainCRTStartup)
        else()
            target_link_options(PakalPlayerWindowed PRIVATE -mwindows)
        endif()
    endif()
endif()

# Mesh viewer: one static 3D model, directional light and sky (needs bgfx + Assimp)
if(EXISTS ${CMAKE_SOURCE_DIR}/examples/mesh_viewer/main.cpp)
    add_executable(PakalMeshViewer ${CMAKE_SOURCE_DIR}/examples/mesh_viewer/main.cpp)
    target_link_libraries(PakalMeshViewer Pakal)
    target_include_directories(PakalMeshViewer PRIVATE ${CMAKE_SOURCE_DIR}/source)
    if(TARGET PakalShaders)   # not there without shaderc: configure goes on, the viewer then has no shaders built
        add_dependencies(PakalMeshViewer PakalShaders)
    endif()
endif()

# Build ContraGame example if bgfx and Box2D are available
if(EXISTS ${CMAKE_SOURCE_DIR}/examples/ContraGame/CMakeLists.txt)
    if(EXISTS ${BGFX_MAIN_LIB} AND BOX2D_FOUND)
        message(STATUS "Building ContraGame example (bgfx + Box2D)")
        add_subdirectory(${CMAKE_SOURCE_DIR}/examples/ContraGame)
    else()
        message(STATUS "Skipping ContraGame example (requires bgfx + Box2D)")
    endif()
endif()

# Physics checks on a Box2D world with level collision data (run with a collision and a spawn file)
if(EXISTS ${CMAKE_SOURCE_DIR}/tests/PhysicsTests.cpp AND BOX2D_FOUND)
    # Links Pakal only: Box2D must exist once, inside the engine library
    add_executable(PhysicsTests ${CMAKE_SOURCE_DIR}/tests/PhysicsTests.cpp)
    target_link_libraries(PhysicsTests Pakal)
    target_include_directories(PhysicsTests PRIVATE ${CMAKE_SOURCE_DIR}/source)
    # its fixture is made from a converted level of the original game, which stays out of the public repository
    set(PAKAL_TEST_FIXTURE ${CMAKE_SOURCE_DIR}/tests/fixtures/hollywood_s01t01)
    if(EXISTS ${PAKAL_TEST_FIXTURE})
        add_test(NAME PhysicsTests COMMAND PhysicsTests ${PAKAL_TEST_FIXTURE}/collision.txt ${PAKAL_TEST_FIXTURE}/spawn.txt)
    endif()
endif()

# Zombies data converter (development tool): level JSON + models -> Pakal level format
if(WIN32)
    find_package(nlohmann_json CONFIG REQUIRED)
    add_executable(ZombiesConvert ${CMAKE_SOURCE_DIR}/tools/zombies_convert/main.cpp)
    target_link_libraries(ZombiesConvert nlohmann_json::nlohmann_json)
    add_executable(LevelReachabilityTests ${CMAKE_SOURCE_DIR}/tests/LevelReachabilityTests.cpp)
    target_link_libraries(LevelReachabilityTests Pakal nlohmann_json::nlohmann_json)
    # It needs a converted level (not in the repository): a game that has one adds the test with it
    # font-effect: outline made fast (source/bgfx/FontOutline.h) against RmlUi's own way
    add_executable(FontOutlineTests ${CMAKE_SOURCE_DIR}/tests/FontOutlineTests.cpp)
    target_include_directories(FontOutlineTests PRIVATE ${CMAKE_SOURCE_DIR}/source)
    add_test(NAME FontOutlineTests COMMAND FontOutlineTests)
    add_executable(ActorOrientationTests ${CMAKE_SOURCE_DIR}/tests/ActorOrientationTests.cpp)
    target_link_libraries(ActorOrientationTests PRIVATE ${BGFX_BUILD_DIR}/bxRelease.lib)
    target_compile_options(ActorOrientationTests PRIVATE /Zc:preprocessor /Zc:__cplusplus)
    target_compile_definitions(ActorOrientationTests PRIVATE BX_CONFIG_DEBUG=0)
    target_include_directories(ActorOrientationTests PRIVATE ${CMAKE_SOURCE_DIR}/source)
    add_test(NAME ActorOrientationTests COMMAND ActorOrientationTests)
    add_test(NAME ZombiesPlacementTests COMMAND ${CMAKE_COMMAND}
        -DCONVERTER=$<TARGET_FILE:ZombiesConvert>
        -DTEST_DIR=${CMAKE_BINARY_DIR}/placement-test
        -P ${CMAKE_SOURCE_DIR}/tests/ZombiesPlacementTests.cmake)
endif()

# The scene's offscreen targets (post-process, upscale) accepted by bgfx's validation, on its Noop renderer (no GPU, no
# window): every format is supported there, so a failure is the engine's flags
if(EXISTS ${BGFX_MAIN_LIB})
    add_executable(SceneTargetTests ${CMAKE_SOURCE_DIR}/tests/SceneTargetTests.cpp)
    target_include_directories(SceneTargetTests PRIVATE ${CMAKE_SOURCE_DIR}/source ${BGFX_DIR}/include)
    target_link_libraries(SceneTargetTests PRIVATE ${BGFX_LIBS} ${BGFX_SYSTEM_LIBS})
    target_compile_definitions(SceneTargetTests PRIVATE BX_CONFIG_DEBUG=0)
    if(MSVC)
        target_compile_options(SceneTargetTests PRIVATE /Zc:preprocessor /Zc:__cplusplus)
        target_include_directories(SceneTargetTests PRIVATE ${CMAKE_SOURCE_DIR}/external/bx/include/compat/msvc)
        target_link_libraries(SceneTargetTests PRIVATE psapi user32 gdi32)
    else()
        target_link_libraries(SceneTargetTests PRIVATE X11 GL pthread)
    endif()
    add_test(NAME SceneTargetTests COMMAND SceneTargetTests)
endif()

# The GPU grass when the frame's instance buffer is nearly full (the tiles that fit drawn, the rest counted), on bgfx's
# Noop renderer with the build's compiled grass shaders
if(EXISTS ${BGFX_MAIN_LIB} AND TARGET PakalShaders)
    # BgfxGrass built in, on the test's own bgfx (the engine's library keeps its bgfx inside); the log and the profiler
    # from the engine's library
    add_executable(GrassInstanceTests ${CMAKE_SOURCE_DIR}/tests/GrassInstanceTests.cpp ${CMAKE_SOURCE_DIR}/source/bgfx/BgfxGrass.cpp)
    target_include_directories(GrassInstanceTests PRIVATE ${CMAKE_SOURCE_DIR}/source ${BGFX_DIR}/include)
    target_link_libraries(GrassInstanceTests PRIVATE Pakal ${BGFX_LIBS} ${BGFX_SYSTEM_LIBS})
    target_compile_definitions(GrassInstanceTests PRIVATE BX_CONFIG_DEBUG=0)
    if(MSVC)
        target_compile_options(GrassInstanceTests PRIVATE /Zc:preprocessor /Zc:__cplusplus)
        target_include_directories(GrassInstanceTests PRIVATE ${CMAKE_SOURCE_DIR}/external/bx/include/compat/msvc)
        target_link_libraries(GrassInstanceTests PRIVATE psapi user32 gdi32)
    else()
        target_link_libraries(GrassInstanceTests PRIVATE X11 GL pthread)
    endif()
    add_dependencies(GrassInstanceTests PakalShaders)
    add_test(NAME GrassInstanceTests COMMAND GrassInstanceTests ${PAKAL_SHADER_DIR})
endif()

# Quests (game_genres/quests, namespace Pakal::Quests; docs/QUESTS.md): what a game's quests show and how they run, for
# any genre. On the core only: it never includes a genre, the UI or graphics (a genre gives a binding, a game a presenter)
file(GLOB PAKAL_QUESTS_SOURCES CONFIGURE_DEPENDS ${CMAKE_SOURCE_DIR}/game_genres/quests/*.cpp)
add_library(PakalQuests STATIC ${PAKAL_QUESTS_SOURCES})
target_link_libraries(PakalQuests PUBLIC Pakal)
target_include_directories(PakalQuests PUBLIC ${CMAKE_SOURCE_DIR}/source ${CMAKE_SOURCE_DIR}/game_genres)
add_executable(QuestArbiterTests ${CMAKE_SOURCE_DIR}/tests/QuestArbiterTests.cpp)
target_link_libraries(QuestArbiterTests PRIVATE PakalQuests)
add_test(NAME QuestArbiterTests COMMAND QuestArbiterTests)
add_executable(QuestSystemTests ${CMAKE_SOURCE_DIR}/tests/QuestSystemTests.cpp)
target_link_libraries(QuestSystemTests PRIVATE PakalQuests)
add_test(NAME QuestSystemTests COMMAND QuestSystemTests)
# The designers' quest templates (game_genres/quests/templates, docs/QUEST_GUIDE.md): each checks clean and plays to its end
add_executable(QuestTemplatesTests ${CMAKE_SOURCE_DIR}/tests/QuestTemplatesTests.cpp)
target_link_libraries(QuestTemplatesTests PRIVATE PakalQuests)
add_test(NAME QuestTemplatesTests COMMAND QuestTemplatesTests ${CMAKE_SOURCE_DIR}/game_genres/quests/templates)
# The quests' development panel (game_genres/quests/view, Dear ImGui through the engine's includes): what a game's F1
# window shows of its quests, and the moves a tester makes on a stuck one. Apart from PakalQuests, which draws nothing
file(GLOB PAKAL_QUESTS_VIEW_SOURCES CONFIGURE_DEPENDS ${CMAKE_SOURCE_DIR}/game_genres/quests/view/*.cpp)
add_library(PakalQuestsView STATIC ${PAKAL_QUESTS_VIEW_SOURCES})
target_link_libraries(PakalQuestsView PUBLIC PakalQuests)

# TopDown: the module of top-down action games (game_genres/topdown, namespace Pakal::TopDown). PakalTopDown is the
# simulation (actors, components, rules, spawns, enemy movement) and draws nothing; PakalTopDownView draws it (character
# models, props, effects, sounds). A game links both; a server or a test only the first. It goes through the engine's
# PhysicsWorld2D: Box2D lives once, inside Pakal.
file(GLOB PAKAL_TOPDOWN_SOURCES CONFIGURE_DEPENDS ${CMAKE_SOURCE_DIR}/game_genres/topdown/*.cpp ${CMAKE_SOURCE_DIR}/game_genres/topdown/kits/*/*.cpp)
add_library(PakalTopDown STATIC ${PAKAL_TOPDOWN_SOURCES})
target_link_libraries(PakalTopDown PUBLIC Pakal PakalLevelDoc)
target_include_directories(PakalTopDown PUBLIC ${CMAKE_SOURCE_DIR}/source ${CMAKE_SOURCE_DIR}/game_genres)
file(GLOB PAKAL_TOPDOWN_VIEW_SOURCES CONFIGURE_DEPENDS ${CMAKE_SOURCE_DIR}/game_genres/topdown/view/*.cpp)
add_library(PakalTopDownView STATIC ${PAKAL_TOPDOWN_VIEW_SOURCES})
target_link_libraries(PakalTopDownView PUBLIC PakalTopDown)
# The level editor (game_genres/topdown/editor): a game module opens it with PAKAL_EDIT=<level> (PakalPlayer <module> --edit
# <level>) and gives it the state that plays a level (Play)
file(GLOB PAKAL_TOPDOWN_EDITOR_SOURCES CONFIGURE_DEPENDS ${CMAKE_SOURCE_DIR}/game_genres/topdown/editor/*.cpp)
add_library(PakalTopDownEditor STATIC ${PAKAL_TOPDOWN_EDITOR_SOURCES})
target_link_libraries(PakalTopDownEditor PUBLIC PakalTopDownView)

# The network: ENet (external/enet, a submodule), the engine's host over it (source/net: PakalNet), and TopDown played
# over it (game_genres/topdown/net: a server, its clients, snapshots; PakalTopDownNet). Draws nothing: a headless server
# links it with PakalTopDown alone.
file(GLOB ENET_SOURCES ${CMAKE_SOURCE_DIR}/external/enet/*.c)
add_library(enet STATIC ${ENET_SOURCES})
target_include_directories(enet PUBLIC ${CMAKE_SOURCE_DIR}/external/enet/include)
if(WIN32)
    target_link_libraries(enet PUBLIC ws2_32 winmm)
endif()
add_library(PakalNet STATIC ${CMAKE_SOURCE_DIR}/source/net/NetHost.cpp ${CMAKE_SOURCE_DIR}/source/net/LanDiscovery.cpp)
target_link_libraries(PakalNet PUBLIC enet)
target_include_directories(PakalNet PUBLIC ${CMAKE_SOURCE_DIR}/source)
file(GLOB PAKAL_TOPDOWN_NET_SOURCES CONFIGURE_DEPENDS ${CMAKE_SOURCE_DIR}/game_genres/topdown/net/*.cpp)
add_library(PakalTopDownNet STATIC ${PAKAL_TOPDOWN_NET_SOURCES})
target_link_libraries(PakalTopDownNet PUBLIC PakalTopDown PakalNet)

# A genre a checkout carries of its own (game_genres/platformer, namespace Pakal::Platformer), built when it is there
file(GLOB PAKAL_PLATFORMER_SOURCES CONFIGURE_DEPENDS ${CMAKE_SOURCE_DIR}/game_genres/platformer/*.cpp)
if(PAKAL_PLATFORMER_SOURCES)
    add_library(PakalPlatformer STATIC ${PAKAL_PLATFORMER_SOURCES})
    target_link_libraries(PakalPlatformer PUBLIC Pakal nlohmann_json::nlohmann_json)
    target_include_directories(PakalPlatformer PUBLIC ${CMAKE_SOURCE_DIR}/source ${CMAKE_SOURCE_DIR}/game_genres)
endif()

# Zombies game module: runs a converted level inside PakalPlayer (PakalPlayer ZombiesModule.dll)
if(WIN32 AND EXISTS ${CMAKE_SOURCE_DIR}/examples/zombies_module/ZombiesModule.cpp)
    set(ZOMBIES_DIR ${CMAKE_SOURCE_DIR}/examples/zombies_module)
    # The game's interface (documents, style sheets, fonts) goes to Assets/ui next to the player, the campaign to Assets/game
    pakal_add_game_module(ZombiesModule
        SOURCES ${ZOMBIES_DIR}/ZombiesModule.cpp ${ZOMBIES_DIR}/ZombiesLevelState.cpp ${ZOMBIES_DIR}/AutoplayBot.cpp
            ${ZOMBIES_DIR}/WorldOverlays.cpp ${ZOMBIES_DIR}/ZombiesDevPanel.cpp ${ZOMBIES_DIR}/ZombiesHud.cpp
            ${ZOMBIES_DIR}/ZombiesScripting.cpp ${ZOMBIES_DIR}/ZombiesLoading.cpp ${ZOMBIES_DIR}/LevelLook.cpp
            ${ZOMBIES_DIR}/CharacterViewer.cpp ${ZOMBIES_DIR}/ZombieScript.cpp ${ZOMBIES_DIR}/ZombiesGame.cpp
            ${ZOMBIES_DIR}/Components.cpp ${ZOMBIES_DIR}/GameRules.cpp ${ZOMBIES_DIR}/Meta.cpp ${ZOMBIES_DIR}/MenuState.cpp
        LINKS Pakal::TopDownView Pakal::TopDownEditor Pakal::Scripting Pakal::LevelDoc nlohmann_json::nlohmann_json
        INCLUDES ${ZOMBIES_DIR}
        DATA ${ZOMBIES_DIR}/ui ui ${ZOMBIES_DIR}/game game
        DATA_TARGET ZombiesUI)
    # Lua runs the level scripts: the engine's ScriptHost (source/scripting) is the only part that links it
    find_package(unofficial-lua CONFIG REQUIRED)
    add_library(PakalScripting STATIC ${CMAKE_SOURCE_DIR}/source/scripting/ScriptHost.cpp)
    target_link_libraries(PakalScripting PUBLIC Pakal unofficial::lua::lua)
    target_include_directories(PakalScripting PUBLIC ${CMAKE_SOURCE_DIR}/source)
    # The simulation without window or level (weapon cycles, enemy timing): checked against the original's numbers
    add_executable(ZombiesGameTests tests/ZombiesGameTests.cpp
        examples/zombies_module/ZombiesGame.cpp examples/zombies_module/Components.cpp
        examples/zombies_module/GameRules.cpp)
    target_link_libraries(ZombiesGameTests PakalTopDown nlohmann_json::nlohmann_json)
    target_include_directories(ZombiesGameTests PRIVATE ${CMAKE_SOURCE_DIR}/source ${CMAKE_SOURCE_DIR}/examples/zombies_module)
    add_test(NAME ZombiesGameTests COMMAND ZombiesGameTests)
    add_executable(ZombieScriptTests tests/ZombieScriptTests.cpp examples/zombies_module/ZombieScript.cpp)
    target_link_libraries(ZombieScriptTests PakalScripting PakalLevelDoc)
    target_include_directories(ZombieScriptTests PRIVATE ${CMAKE_SOURCE_DIR}/source)
    add_test(NAME ZombieScriptTests COMMAND ZombieScriptTests)
    # What lasts between missions: campaign, rewards, shop, loadout, the saved profile
    add_executable(MetaTests tests/MetaTests.cpp examples/zombies_module/Meta.cpp examples/zombies_module/GameRules.cpp)
    target_link_libraries(MetaTests PakalTopDown nlohmann_json::nlohmann_json)
    target_include_directories(MetaTests PRIVATE ${CMAKE_SOURCE_DIR}/source ${CMAKE_SOURCE_DIR}/examples/zombies_module)
    add_test(NAME MetaTests COMMAND MetaTests ${CMAKE_SOURCE_DIR}/examples/zombies_module/game/campaign.json)
    add_executable(CombatRulesTests tests/CombatRulesTests.cpp)
    target_link_libraries(CombatRulesTests PakalTopDown)
    add_test(NAME CombatRulesTests COMMAND CombatRulesTests)
    add_executable(FoliageRulesTests tests/FoliageRulesTests.cpp)
    target_link_libraries(FoliageRulesTests PakalTopDownView nlohmann_json::nlohmann_json)
    add_test(NAME FoliageRulesTests COMMAND FoliageRulesTests)
    # a look made of parts: its "recipe" and "recipes" read inline or from a recipe file (no GPU)
    add_executable(LookRecipeTests tests/LookRecipeTests.cpp)
    target_link_libraries(LookRecipeTests PakalTopDownView nlohmann_json::nlohmann_json)
    add_test(NAME LookRecipeTests COMMAND LookRecipeTests)
    add_executable(CameraInputTests tests/CameraInputTests.cpp)
    target_include_directories(CameraInputTests PRIVATE ${CMAKE_SOURCE_DIR}/source)
    add_executable(ZombieRulesTests tests/ZombieRulesTests.cpp examples/zombies_module/GameRules.cpp game_genres/topdown/TopDownRules.cpp)
    target_link_libraries(ZombieRulesTests nlohmann_json::nlohmann_json)
    target_include_directories(ZombieRulesTests PRIVATE ${CMAKE_SOURCE_DIR}/source ${CMAKE_SOURCE_DIR}/examples/zombies_module)
    add_test(NAME ZombieRulesTests COMMAND ZombieRulesTests)
    add_test(NAME CameraInputTests COMMAND CameraInputTests)
    # The level reachability check on the Zombies example's converted level, when it is next to the player
    set(ZOMBIES_REACHABILITY_LEVEL ${CMAKE_RUNTIME_OUTPUT_DIRECTORY}/Assets/zombies/hollywood_s01t01/level.json)
    if(TARGET LevelReachabilityTests AND EXISTS ${ZOMBIES_REACHABILITY_LEVEL})
        add_test(NAME LevelReachabilityTests COMMAND LevelReachabilityTests ${ZOMBIES_REACHABILITY_LEVEL})
    endif()
endif()

# Brawler prototype: a second game on TopDown (one arena, hand to hand), to try the module. Its simulation is tested
# without a window; the module runs in PakalPlayer (PakalPlayer BrawlerModule.dll) with the Zombies example's models.
if(WIN32 AND EXISTS ${CMAKE_SOURCE_DIR}/examples/brawler_proto/BrawlerGame.cpp)
    # its rules go next to the player, in Assets/brawler
    set(BRAWLER_DIR ${CMAKE_SOURCE_DIR}/examples/brawler_proto)
    pakal_add_game_module(BrawlerModule
        SOURCES ${BRAWLER_DIR}/BrawlerModule.cpp ${BRAWLER_DIR}/BrawlerLevelState.cpp ${BRAWLER_DIR}/BrawlerGame.cpp
        LINKS Pakal::TopDownView nlohmann_json::nlohmann_json
        INCLUDES ${BRAWLER_DIR}
        DATA ${BRAWLER_DIR}/data brawler
        DATA_TARGET BrawlerData)
    add_executable(BrawlerTests tests/BrawlerTests.cpp examples/brawler_proto/BrawlerGame.cpp)
    target_link_libraries(BrawlerTests PakalTopDown nlohmann_json::nlohmann_json)
    target_include_directories(BrawlerTests PRIVATE ${CMAKE_SOURCE_DIR}/source ${CMAKE_SOURCE_DIR}/examples/brawler_proto)
    add_test(NAME BrawlerTests COMMAND BrawlerTests ${CMAKE_SOURCE_DIR}/examples/brawler_proto/data/brawler.json)
endif()

# Dungeon prototype: a small Diablo-like on TopDown and its RPG kit (rooms made from a seed, abilities, loot, a boss). Its
# simulation is tested without a window; the module runs in PakalPlayer (PakalPlayer DungeonModule.dll). The dungeon's
# pieces come from the local Medieval Village pack (not in Git), copied next to the player when it is there.
if(WIN32 AND EXISTS ${CMAKE_SOURCE_DIR}/examples/dungeon_proto/DungeonGame.cpp)
    # (its data is copied by DungeonData below: the pieces of a local pack go with it)
    set(DUNGEON_DIR ${CMAKE_SOURCE_DIR}/examples/dungeon_proto)
    pakal_add_game_module(DungeonModule
        SOURCES ${DUNGEON_DIR}/DungeonModule.cpp ${DUNGEON_DIR}/DungeonLevelState.cpp ${DUNGEON_DIR}/DungeonMenuState.cpp
            ${DUNGEON_DIR}/DungeonGame.cpp ${DUNGEON_DIR}/DungeonNet.cpp
        LINKS Pakal::TopDownView Pakal::TopDownNet nlohmann_json::nlohmann_json
        INCLUDES ${DUNGEON_DIR})
    set(DUNGEON_PIECES_SOURCE ${CMAKE_SOURCE_DIR}/examples/zombies_module/assets/source/medieval-village/glTF)
    set(DUNGEON_PIECES Floor_UnevenBrick Wall_UnevenBrick_Straight Prop_Crate Prop_Brick1 Prop_Brick2 Prop_Support)
    set(DUNGEON_TEXTURES T_UnevenBrick_BaseColor T_UnevenBrick_Normal T_UnevenBrick_Roughness T_WoodTrim_BaseColor
        T_WoodTrim_Normal T_WoodTrim_Roughness T_Plaster_BaseColor T_Plaster_Normal T_Plaster_ORM T_Brick_BaseColor
        T_Brick_Normal T_Brick_Roughness T_RockTrim_BaseColor T_RockTrim_Normal T_RockTrim_ORM)
    set(DUNGEON_PIECE_FILES)
    set(DUNGEON_COPY_PIECES)
    if(EXISTS ${DUNGEON_PIECES_SOURCE})
        foreach(piece ${DUNGEON_PIECES})
            list(APPEND DUNGEON_PIECE_FILES ${DUNGEON_PIECES_SOURCE}/${piece}.gltf ${DUNGEON_PIECES_SOURCE}/${piece}.bin)
        endforeach()
        foreach(texture ${DUNGEON_TEXTURES})
            list(APPEND DUNGEON_PIECE_FILES ${DUNGEON_PIECES_SOURCE}/${texture}.png)
        endforeach()
        set(DUNGEON_COPY_PIECES COMMAND ${CMAKE_COMMAND} -E copy_if_different ${DUNGEON_PIECE_FILES} ${CMAKE_RUNTIME_OUTPUT_DIRECTORY}/Assets/dungeon/meshes)
    else()
        message(STATUS "Dungeon prototype: no Medieval Village pack in ${DUNGEON_PIECES_SOURCE}; the dungeon has no walls to draw")
    endif()
    file(GLOB_RECURSE DUNGEON_DATA_FILES CONFIGURE_DEPENDS ${CMAKE_SOURCE_DIR}/examples/dungeon_proto/data/*)
    add_custom_target(DungeonData ALL
        COMMAND ${CMAKE_COMMAND} -E copy_directory ${CMAKE_SOURCE_DIR}/examples/dungeon_proto/data ${CMAKE_RUNTIME_OUTPUT_DIRECTORY}/Assets/dungeon
        COMMAND ${CMAKE_COMMAND} -E make_directory ${CMAKE_RUNTIME_OUTPUT_DIRECTORY}/Assets/dungeon/meshes
        ${DUNGEON_COPY_PIECES}
        DEPENDS ${DUNGEON_DATA_FILES})
    add_dependencies(DungeonModule DungeonData)
    add_executable(DungeonTests tests/DungeonTests.cpp examples/dungeon_proto/DungeonGame.cpp)
    target_link_libraries(DungeonTests PakalTopDown nlohmann_json::nlohmann_json)
    target_include_directories(DungeonTests PRIVATE ${CMAKE_SOURCE_DIR}/source ${CMAKE_SOURCE_DIR}/examples/dungeon_proto)
    add_test(NAME DungeonTests COMMAND DungeonTests ${CMAKE_SOURCE_DIR}/examples/dungeon_proto/data/dungeon.json)
    # the headless server (no window: the simulation and the network), and the network's test (a server and two clients
    # over UDP on this machine)
    add_executable(DungeonServer examples/dungeon_proto/DungeonServerMain.cpp examples/dungeon_proto/DungeonGame.cpp examples/dungeon_proto/DungeonNet.cpp)
    target_link_libraries(DungeonServer PakalTopDownNet nlohmann_json::nlohmann_json)
    target_include_directories(DungeonServer PRIVATE ${CMAKE_SOURCE_DIR}/source ${CMAKE_SOURCE_DIR}/examples/dungeon_proto)
    add_executable(NetTests tests/NetTests.cpp examples/dungeon_proto/DungeonGame.cpp examples/dungeon_proto/DungeonNet.cpp)
    target_link_libraries(NetTests PakalTopDownNet nlohmann_json::nlohmann_json)
    target_include_directories(NetTests PRIVATE ${CMAKE_SOURCE_DIR}/source ${CMAKE_SOURCE_DIR}/examples/dungeon_proto)
    add_test(NAME NetTests COMMAND NetTests ${CMAKE_SOURCE_DIR}/examples/dungeon_proto/data/dungeon.json)
endif()

# Level document: versioned level data shared by the game module, the converter and the editor
# (no engine dependency)
# and its editor (operations with undo), and pakal-level, the level's command line for people and agents
add_library(PakalLevelDoc STATIC ${CMAKE_SOURCE_DIR}/tools/level_doc/LevelDocument.cpp ${CMAKE_SOURCE_DIR}/tools/level_doc/LevelEditor.cpp
    ${CMAKE_SOURCE_DIR}/tools/level_doc/PrefabLibrary.cpp ${CMAKE_SOURCE_DIR}/tools/level_doc/LevelLayers.cpp
    ${CMAKE_SOURCE_DIR}/tools/level_doc/LiveEditing.cpp
    ${CMAKE_SOURCE_DIR}/tools/level_doc/LevelLayerEditing.cpp)
find_package(nlohmann_json CONFIG REQUIRED)
target_link_libraries(PakalLevelDoc PUBLIC nlohmann_json::nlohmann_json)
target_include_directories(PakalLevelDoc PUBLIC ${CMAKE_SOURCE_DIR}/tools/level_doc)
add_executable(pakal-level ${CMAKE_SOURCE_DIR}/tools/level_doc/pakal_level.cpp)
target_link_libraries(pakal-level PakalLevelDoc)

if(EXISTS ${CMAKE_SOURCE_DIR}/tests/LevelDocumentTests.cpp)
    add_executable(LevelDocumentTests ${CMAKE_SOURCE_DIR}/tests/LevelDocumentTests.cpp)
    target_link_libraries(LevelDocumentTests PakalLevelDoc)
    add_test(NAME LevelDocumentTests COMMAND LevelDocumentTests)
endif()
add_executable(LevelEditorTests ${CMAKE_SOURCE_DIR}/tests/LevelEditorTests.cpp)
target_link_libraries(LevelEditorTests PakalLevelDoc)
add_test(NAME LevelEditorTests COMMAND LevelEditorTests)
add_executable(PrefabTests ${CMAKE_SOURCE_DIR}/tests/PrefabTests.cpp)
target_link_libraries(PrefabTests PakalLevelDoc)
add_test(NAME PrefabTests COMMAND PrefabTests)
add_executable(LevelLayerEditingTests ${CMAKE_SOURCE_DIR}/tests/LevelLayerEditingTests.cpp)
target_link_libraries(LevelLayerEditingTests PakalLevelDoc)
target_include_directories(LevelLayerEditingTests PRIVATE ${CMAKE_SOURCE_DIR}/game_genres)
add_test(NAME LevelLayerEditingTests COMMAND LevelLayerEditingTests)
add_executable(LiveEditingTests ${CMAKE_SOURCE_DIR}/tests/LiveEditingTests.cpp)
target_link_libraries(LiveEditingTests PakalLevelDoc)
add_test(NAME LiveEditingTests COMMAND LiveEditingTests)

# Gameplay core on level data: navigation, enemy chase, weapons, objective, pickups
if(EXISTS ${CMAKE_SOURCE_DIR}/tests/GameplayTests.cpp)
    add_executable(GameplayTests ${CMAKE_SOURCE_DIR}/tests/GameplayTests.cpp)
    target_link_libraries(GameplayTests PakalTopDown)
    target_include_directories(GameplayTests PRIVATE ${CMAKE_SOURCE_DIR}/source)
    set(PAKAL_TEST_FIXTURE ${CMAKE_SOURCE_DIR}/tests/fixtures/hollywood_s01t01)
    if(EXISTS ${PAKAL_TEST_FIXTURE})
        add_test(NAME GameplayTests COMMAND GameplayTests ${PAKAL_TEST_FIXTURE}/collision.txt ${PAKAL_TEST_FIXTURE}/spawn.txt ${PAKAL_TEST_FIXTURE}/entities.txt)
    endif()
endif()

# Audio demo: tone from four positions around the listener (listen to check direction)
if(EXISTS ${CMAKE_SOURCE_DIR}/examples/audio_demo/main.cpp)
    add_executable(PakalAudioDemo ${CMAKE_SOURCE_DIR}/examples/audio_demo/main.cpp)
    target_link_libraries(PakalAudioDemo Pakal)
    target_include_directories(PakalAudioDemo PRIVATE ${CMAKE_SOURCE_DIR}/source)
endif()

# Spatial audio math checks (no audio device needed)
if(EXISTS ${CMAKE_SOURCE_DIR}/tests/AudioTests.cpp)
    add_executable(AudioTests ${CMAKE_SOURCE_DIR}/tests/AudioTests.cpp)
    target_link_libraries(AudioTests Pakal)
    target_include_directories(AudioTests PRIVATE ${CMAKE_SOURCE_DIR}/source)
    target_compile_definitions(AudioTests PRIVATE PAKAL_TEST_FIXTURES="${CMAKE_SOURCE_DIR}/tests/fixtures")
    add_test(NAME AudioTests COMMAND AudioTests)
endif()

# Prefab resolver unit tests (no engine startup required)
if(EXISTS ${CMAKE_SOURCE_DIR}/tests/PrefabResolverTests.cpp)
    add_executable(PrefabResolverTests ${CMAKE_SOURCE_DIR}/tests/PrefabResolverTests.cpp)
    target_link_libraries(PrefabResolverTests Pakal)
    target_include_directories(PrefabResolverTests PRIVATE ${CMAKE_SOURCE_DIR}/source)
    add_test(NAME PrefabResolverTests COMMAND PrefabResolverTests)
    message(STATUS "Building PrefabResolverTests")
endif()

# The simulation's core pieces (header-only, no engine startup)
add_executable(SimulationClockTests ${CMAKE_SOURCE_DIR}/tests/SimulationClockTests.cpp)
target_include_directories(SimulationClockTests PRIVATE ${CMAKE_SOURCE_DIR}/source)
# (the clock follows the engine's time scale, TimeScale.cpp)
target_link_libraries(SimulationClockTests PRIVATE PakalCore)
add_test(NAME SimulationClockTests COMMAND SimulationClockTests)
add_executable(RandomStreamsTests ${CMAKE_SOURCE_DIR}/tests/RandomStreamsTests.cpp)
target_include_directories(RandomStreamsTests PRIVATE ${CMAKE_SOURCE_DIR}/source)
add_test(NAME RandomStreamsTests COMMAND RandomStreamsTests)
add_executable(CollisionLayersTests ${CMAKE_SOURCE_DIR}/tests/CollisionLayersTests.cpp)
target_include_directories(CollisionLayersTests PRIVATE ${CMAKE_SOURCE_DIR}/source)
add_test(NAME CollisionLayersTests COMMAND CollisionLayersTests)
add_executable(SimWorldTests ${CMAKE_SOURCE_DIR}/tests/SimWorldTests.cpp)
target_link_libraries(SimWorldTests Pakal)
target_include_directories(SimWorldTests PRIVATE ${CMAKE_SOURCE_DIR}/source)
add_test(NAME SimWorldTests COMMAND SimWorldTests)
add_executable(AssetCacheTests ${CMAKE_SOURCE_DIR}/tests/AssetCacheTests.cpp)
target_include_directories(AssetCacheTests PRIVATE ${CMAKE_SOURCE_DIR}/source)
add_test(NAME AssetCacheTests COMMAND AssetCacheTests)
# a sealed pack read by its names, also under the resource manager's root (ZipSourceMiniz is built in, not exported)
add_executable(ZipSourceTests ${CMAKE_SOURCE_DIR}/tests/ZipSourceTests.cpp ${CMAKE_SOURCE_DIR}/source/bgfx/ZipSourceMiniz.cpp ${PAKAL_MINIZ_SOURCES})
target_include_directories(ZipSourceTests PRIVATE ${CMAKE_SOURCE_DIR}/source ${CMAKE_SOURCE_DIR}/external/bimg/3rdparty/tinyexr/deps/miniz)
target_link_libraries(ZipSourceTests Pakal ${BGFX_LIBS} ${BGFX_SYSTEM_LIBS} ${PAKAL_MINIZ_LIBS})
add_test(NAME ZipSourceTests COMMAND ZipSourceTests)
# A character made of parts (CharacterRecipe, CharacterAssembly): two tiny glTF parts written by the test itself
add_executable(CharacterAssemblyTests ${CMAKE_SOURCE_DIR}/tests/CharacterAssemblyTests.cpp)
target_include_directories(CharacterAssemblyTests PRIVATE ${CMAKE_SOURCE_DIR}/source)
target_link_libraries(CharacterAssemblyTests Pakal)
add_test(NAME CharacterAssemblyTests COMMAND CharacterAssemblyTests)
# a model's shape (graphics/ModelShape.h) as the loader fills it: a split tree's crown base, none without a split (no GPU)
add_executable(ModelShapeTests ${CMAKE_SOURCE_DIR}/tests/ModelShapeTests.cpp)
target_include_directories(ModelShapeTests PRIVATE ${CMAKE_SOURCE_DIR}/source)
target_link_libraries(ModelShapeTests Pakal)
add_test(NAME ModelShapeTests COMMAND ModelShapeTests)
add_executable(EventBusTests ${CMAKE_SOURCE_DIR}/tests/EventBusTests.cpp)
target_include_directories(EventBusTests PRIVATE ${CMAKE_SOURCE_DIR}/source)
add_test(NAME EventBusTests COMMAND EventBusTests)
add_executable(ActionTests ${CMAKE_SOURCE_DIR}/tests/ActionTests.cpp)
target_include_directories(ActionTests PRIVATE ${CMAKE_SOURCE_DIR}/source)
add_test(NAME ActionTests COMMAND ActionTests)
add_executable(AnimationMarksTests ${CMAKE_SOURCE_DIR}/tests/AnimationMarksTests.cpp)
target_include_directories(AnimationMarksTests PRIVATE ${CMAKE_SOURCE_DIR}/source)
add_test(NAME AnimationMarksTests COMMAND AnimationMarksTests)
# a frame's posing shared among threads (WorkShare) and the marks held while posed on them (MarkReports)
add_executable(PoseSharingTests ${CMAKE_SOURCE_DIR}/tests/PoseSharingTests.cpp)
target_include_directories(PoseSharingTests PRIVATE ${CMAKE_SOURCE_DIR}/source)
find_package(Threads REQUIRED)
target_link_libraries(PoseSharingTests Threads::Threads)
add_test(NAME PoseSharingTests COMMAND PoseSharingTests)
add_executable(RpgKitTests ${CMAKE_SOURCE_DIR}/tests/RpgKitTests.cpp)
target_link_libraries(RpgKitTests PakalTopDown)
add_test(NAME RpgKitTests COMMAND RpgKitTests)
add_executable(ComponentSchemaTests ${CMAKE_SOURCE_DIR}/tests/ComponentSchemaTests.cpp)
target_link_libraries(ComponentSchemaTests PakalTopDown)
add_test(NAME ComponentSchemaTests COMMAND ComponentSchemaTests)
# The engine's MCP server and client over a socket (source/mcp)
add_executable(McpServerTests ${CMAKE_SOURCE_DIR}/tests/McpServerTests.cpp)
target_link_libraries(McpServerTests Pakal)
add_test(NAME McpServerTests COMMAND McpServerTests)
# The tool server's safety without sockets (it runs where McpServerTests, which needs a free port, is left out): a removed
# tool's running call ends first; no tools in a players' copy
add_executable(ToolLifetimeTests ${CMAKE_SOURCE_DIR}/tests/ToolLifetimeTests.cpp)
target_link_libraries(ToolLifetimeTests Pakal)
add_test(NAME ToolLifetimeTests COMMAND ToolLifetimeTests)
# The QA hooks (source/mcp/QaTools: input, state, log, time scale, quit) on a headless engine: the core with its dummy
# systems, the MCP server compiled in beside them
add_executable(QaToolsTests ${CMAKE_SOURCE_DIR}/tests/QaToolsTests.cpp ${CMAKE_SOURCE_DIR}/source/mcp/QaTools.cpp
    ${CMAKE_SOURCE_DIR}/source/mcp/McpServer.cpp ${CMAKE_SOURCE_DIR}/source/mcp/McpClient.cpp)
target_link_libraries(QaToolsTests PRIVATE PakalCore nlohmann_json::nlohmann_json)
if(WIN32)
    target_link_libraries(QaToolsTests PRIVATE ws2_32)
endif()
add_test(NAME QaToolsTests COMMAND QaToolsTests)

# Games kept outside the engine's examples (a folder only some checkouts have)
if(TARGET PakalScripting AND EXISTS ${CMAKE_SOURCE_DIR}/games/CMakeLists.txt)
    add_subdirectory(${CMAKE_SOURCE_DIR}/games)
add_executable(SpriteAtlasTests ${CMAKE_SOURCE_DIR}/tests/SpriteAtlasTests.cpp)
target_link_libraries(SpriteAtlasTests Pakal)
target_include_directories(SpriteAtlasTests PRIVATE ${CMAKE_SOURCE_DIR}/source)
add_test(NAME SpriteAtlasTests COMMAND SpriteAtlasTests)
endif()

# Print configuration summary
message(STATUS "========== Pakal Engine Configuration ==========")
message(STATUS "C++ Standard: ${CMAKE_CXX_STANDARD}")
message(STATUS "Build Type: ${CMAKE_BUILD_TYPE}")
message(STATUS "SFML: ${SFML_FOUND}")
message(STATUS "Box2D: ${BOX2D_FOUND}")
message(STATUS "Lua: ${LUA_FOUND}")
message(STATUS "================================================")

# Rolling frame timing is available without profiling recordings.
add_executable(FrameTimingTests tests/FrameTimingTests.cpp)
target_include_directories(FrameTimingTests PRIVATE ${CMAKE_SOURCE_DIR}/source)
add_test(NAME FrameTimingTests COMMAND FrameTimingTests)

# Games that stay on this machine (cmake/LocalGames.cmake is not in the repository): their targets, if the file is there
include(${CMAKE_SOURCE_DIR}/cmake/LocalGames.cmake OPTIONAL)

# every library is defined: their names for the games (Pakal::TopDownView...; cmake/PakalGameModule.cmake)
pakal_name_targets()
