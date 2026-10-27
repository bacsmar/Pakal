cmake_minimum_required(VERSION 3.10)
# Use CMAKE_MSVC_RUNTIME_LIBRARY (needed for the runtime selection below).
if(POLICY CMP0091)
    cmake_policy(SET CMP0091 NEW)
endif()

project(Pakal)

# Dynamic CRT on MSVC: bgfx is built with /MD (genie --with-dynamic-runtime) and
# the vcpkg packages use the x64-windows-static-md triplet (see CMakePresets.json).
if(MSVC)
    set(CMAKE_MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>DLL")
endif()

# Set C++ standard
set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Set output directories
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin)
set(CMAKE_LIBRARY_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/lib)
set(CMAKE_ARCHIVE_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/lib)

# Tests run with ctest (ctest --test-dir build-win64 --output-on-failure)
enable_testing()

# Create a config.h compatibility header for case-insensitive systems.
# Not on Windows: there it would resolve to itself, since the filesystem ignores case.
if(NOT WIN32)
    file(WRITE "${CMAKE_BINARY_DIR}/config.h" "#include \"Config.h\"\n")
endif()

# Include directories
include_directories(${CMAKE_BINARY_DIR})
include_directories(${CMAKE_SOURCE_DIR}/source)
include_directories(${CMAKE_SOURCE_DIR}/external/bx/include)
include_directories(${CMAKE_SOURCE_DIR}/external/bgfx/include)
include_directories(${CMAKE_SOURCE_DIR}/external/bimg/include)
include_directories(${CMAKE_SOURCE_DIR}/external/bgfx/examples/common)

# Add source files
file(GLOB_RECURSE PAKAL_SOURCES
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

# Platform-specific source files
if(UNIX AND NOT APPLE)
    list(APPEND PAKAL_SOURCES ${CMAKE_SOURCE_DIR}/source/unix/ClockImpl.cpp)
elseif(APPLE)
    list(APPEND PAKAL_SOURCES ${CMAKE_SOURCE_DIR}/source/unix/ClockImpl.cpp)
elseif(WIN32)
    list(APPEND PAKAL_SOURCES ${CMAKE_SOURCE_DIR}/source/win32/ClockImpl.cpp)
endif()

# Create the main library.
# On Windows it is a DLL so that game modules (ContraGameModule.dll) share one
# instance of the engine singletons with PakalPlayer.exe. Elsewhere it stays static.
if(WIN32)
    add_library(Pakal SHARED ${PAKAL_SOURCES})
    set_target_properties(Pakal PROPERTIES WINDOWS_EXPORT_ALL_SYMBOLS ON)
    # Building the engine: export the PAKAL_API_* symbols; consumers import them.
    target_compile_definitions(Pakal PRIVATE PAKAL_NONCLIENT_BUILD)
else()
    add_library(Pakal STATIC ${PAKAL_SOURCES})
endif()
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

# SFML
if(WIN32)
    # vcpkg provides SFML 2.6. The code uses the SFML 2 event API (pollEvent + sf::Event::Closed),
    # which SFML 3 removed. Static SFML also pulls in its dependencies through its config.
    set(SFML_STATIC_LIBRARIES TRUE)
    find_package(SFML 2.6 COMPONENTS system window audio REQUIRED)
    target_link_libraries(Pakal PRIVATE sfml-system sfml-window sfml-audio)
    # SoundManagerSFML.h exposes sf:: types: headers are PUBLIC, the libraries stay PRIVATE.
    # SFML is static here, so consumers must not expect dllimport.
    get_target_property(PAKAL_SFML_INCLUDES sfml-audio INTERFACE_INCLUDE_DIRECTORIES)
    target_include_directories(Pakal PUBLIC ${PAKAL_SFML_INCLUDES})
    target_compile_definitions(Pakal PUBLIC SFML_STATIC)
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
    # Built via makefile in Dockerfile
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

# Engine shaders for the 3D mesh and sky programs (Direct3D 11 on Windows)
if(WIN32)
    set(PAKAL_SHADERC ${BGFX_DIR}/.build/win64_vs2022/bin/shadercRelease.exe)
    set(PAKAL_SHADER_SRC ${CMAKE_SOURCE_DIR}/source/bgfx/shaders)
    set(PAKAL_SHADER_DIR ${CMAKE_RUNTIME_OUTPUT_DIRECTORY}/shaders/dx11)
    if(EXISTS ${PAKAL_SHADERC})
        function(pakal_compile_shader name type varying)
            set(out ${PAKAL_SHADER_DIR}/${name}.bin)
            add_custom_command(OUTPUT ${out}
                COMMAND ${CMAKE_COMMAND} -E make_directory ${PAKAL_SHADER_DIR}
                COMMAND ${PAKAL_SHADERC}
                    -f ${PAKAL_SHADER_SRC}/${name}.sc
                    -o ${out}
                    --type ${type} --platform windows -p s_5_0
                    --varyingdef ${PAKAL_SHADER_SRC}/${varying}
                    -i ${BGFX_DIR}/src
                    -i ${PAKAL_SHADER_SRC}
                DEPENDS ${PAKAL_SHADER_SRC}/${name}.sc ${PAKAL_SHADER_SRC}/${varying} ${PAKAL_SHADER_SRC}/pakal_lighting.sh ${PAKAL_SHADER_SRC}/pakal_precision.sh
                VERBATIM)
            set(PAKAL_SHADER_OUTPUTS ${PAKAL_SHADER_OUTPUTS} ${out} PARENT_SCOPE)
        endfunction()

        pakal_compile_shader(vs_mesh vertex varying_mesh.def.sc)
        pakal_compile_shader(vs_skin vertex varying_skin.def.sc)
        pakal_compile_shader(vs_pbr vertex varying_pbr.def.sc)
        pakal_compile_shader(fs_pbr fragment varying_pbr.def.sc)
        pakal_compile_shader(fs_mesh fragment varying_mesh.def.sc)
        pakal_compile_shader(vs_sky vertex varying_sky.def.sc)
        pakal_compile_shader(fs_sky fragment varying_sky.def.sc)
        pakal_compile_shader(vs_shadow vertex varying_shadow.def.sc)
        pakal_compile_shader(vs_shadow_skin vertex varying_shadow.def.sc)
        pakal_compile_shader(fs_shadow fragment varying_shadow.def.sc)
        pakal_compile_shader(vs_post vertex varying_post.def.sc)
        pakal_compile_shader(fs_post fragment varying_post.def.sc)
        pakal_compile_shader(fs_fsr_easu fragment varying_post.def.sc)
        pakal_compile_shader(fs_fsr_rcas fragment varying_post.def.sc)
        pakal_compile_shader(vs_particle vertex varying_particle.def.sc)
        pakal_compile_shader(fs_particle fragment varying_particle.def.sc)
        pakal_compile_shader(vs_ui vertex varying_ui.def.sc)
        pakal_compile_shader(fs_ui fragment varying_ui.def.sc)

        add_custom_target(PakalShaders ALL DEPENDS ${PAKAL_SHADER_OUTPUTS})
        add_dependencies(Pakal PakalShaders)
    else()
        message(WARNING "shadercRelease.exe not found at ${PAKAL_SHADERC}; 3D meshes will not render")
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

# Box2D physics library (optional). Its headers are part of the public API (Box2DQueries.h,
# CharacterController2D.h), so the include paths are PUBLIC; the library stays PRIVATE.
if(WIN32)
    find_package(box2d CONFIG REQUIRED)
    target_link_libraries(Pakal PRIVATE box2d::box2d)
    get_target_property(PAKAL_BOX2D_INCLUDES box2d::box2d INTERFACE_INCLUDE_DIRECTORIES)
    target_include_directories(Pakal PUBLIC ${PAKAL_BOX2D_INCLUDES})
    set(BOX2D_FOUND TRUE)
    message(STATUS "✓ Box2D found via vcpkg")
else()
    pkg_check_modules(BOX2D box2d)
    if(BOX2D_FOUND)
        message(STATUS "✓ Box2D found via pkg-config")
        target_include_directories(Pakal PUBLIC ${BOX2D_INCLUDE_DIRS})
        target_link_directories(Pakal PRIVATE ${BOX2D_LIBRARY_DIRS})
        target_link_libraries(Pakal PRIVATE ${BOX2D_LIBRARIES})
    else()
        message(STATUS "⚠️  Box2D not found. Install with: sudo apt-get install libbox2d-dev")
    endif()
endif()

# Lua (optional, for scripting)
find_package(Lua QUIET)
if(LUA_FOUND)
    target_include_directories(Pakal PRIVATE ${LUA_INCLUDE_DIR})
    target_link_libraries(Pakal PRIVATE ${LUA_LIBRARIES})
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

if(EXISTS ${CMAKE_SOURCE_DIR}/examples/engine_feature_lab/main.cpp)
    add_executable(PakalFeatureLab ${CMAKE_SOURCE_DIR}/examples/engine_feature_lab/main.cpp)
    target_link_libraries(PakalFeatureLab Pakal)
    target_include_directories(PakalFeatureLab PRIVATE ${CMAKE_SOURCE_DIR}/source)
endif()

# Build generic runtime host for game modules
if(EXISTS ${CMAKE_SOURCE_DIR}/source/PakalPlayerMain.cpp)
    add_executable(PakalPlayer
        ${CMAKE_SOURCE_DIR}/source/PakalPlayerMain.cpp
        ${CMAKE_SOURCE_DIR}/source/resources/SceneLoader.cpp
    )
    target_link_libraries(PakalPlayer Pakal)
    target_include_directories(PakalPlayer PRIVATE ${CMAKE_SOURCE_DIR}/source)
    if(UNIX AND NOT APPLE)
        target_link_options(PakalPlayer PRIVATE -Wl,-E)
    endif()
    set_target_properties(PakalPlayer PROPERTIES ENABLE_EXPORTS ON)
endif()

# Mesh viewer: one static 3D model, directional light and sky (needs bgfx + Assimp)
if(EXISTS ${CMAKE_SOURCE_DIR}/examples/mesh_viewer/main.cpp)
    add_executable(PakalMeshViewer ${CMAKE_SOURCE_DIR}/examples/mesh_viewer/main.cpp)
    target_link_libraries(PakalMeshViewer Pakal)
    target_include_directories(PakalMeshViewer PRIVATE ${CMAKE_SOURCE_DIR}/source)
    if(WIN32)
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
    # Needs a converted level (not in the repository): runs when one is next to the player
    set(PAKAL_REACHABILITY_LEVEL ${CMAKE_RUNTIME_OUTPUT_DIRECTORY}/Assets/zombies/hollywood_s01t01/level.json)
    if(EXISTS ${PAKAL_REACHABILITY_LEVEL})
        add_test(NAME LevelReachabilityTests COMMAND LevelReachabilityTests ${PAKAL_REACHABILITY_LEVEL})
    endif()
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

# Zombies game module: runs a converted level inside PakalPlayer (PakalPlayer ZombiesModule.dll)
if(WIN32 AND EXISTS ${CMAKE_SOURCE_DIR}/examples/zombies_module/ZombiesModule.cpp)
    add_library(ZombiesModule SHARED
        ${CMAKE_SOURCE_DIR}/examples/zombies_module/ZombiesModule.cpp
        ${CMAKE_SOURCE_DIR}/examples/zombies_module/ZombiesLevelState.cpp
        ${CMAKE_SOURCE_DIR}/examples/zombies_module/AutoplayBot.cpp
        ${CMAKE_SOURCE_DIR}/examples/zombies_module/WorldOverlays.cpp
        ${CMAKE_SOURCE_DIR}/examples/zombies_module/ZombiesDevPanel.cpp
        ${CMAKE_SOURCE_DIR}/examples/zombies_module/ZombiesHud.cpp
        ${CMAKE_SOURCE_DIR}/examples/zombies_module/ZombiesScripting.cpp
        ${CMAKE_SOURCE_DIR}/examples/zombies_module/ZombiesLoading.cpp
        ${CMAKE_SOURCE_DIR}/examples/zombies_module/LevelLook.cpp
        ${CMAKE_SOURCE_DIR}/examples/zombies_module/CharacterViewer.cpp
        ${CMAKE_SOURCE_DIR}/examples/zombies_module/ZombieScript.cpp
        ${CMAKE_SOURCE_DIR}/examples/zombies_module/ZombiesGame.cpp
        ${CMAKE_SOURCE_DIR}/examples/zombies_module/Components.cpp
        ${CMAKE_SOURCE_DIR}/examples/zombies_module/GameRules.cpp
        ${CMAKE_SOURCE_DIR}/examples/zombies_module/Meta.cpp
        ${CMAKE_SOURCE_DIR}/examples/zombies_module/MenuState.cpp
    )
    # Lua runs the level scripts: the engine's ScriptHost (source/scripting) is the only part that links it
    find_package(unofficial-lua CONFIG REQUIRED)
    add_library(PakalScripting STATIC ${CMAKE_SOURCE_DIR}/source/scripting/ScriptHost.cpp)
    target_link_libraries(PakalScripting PUBLIC Pakal unofficial::lua::lua)
    target_include_directories(PakalScripting PUBLIC ${CMAKE_SOURCE_DIR}/source)
    target_link_libraries(ZombiesModule PakalTopDownView PakalScripting PakalLevelDoc nlohmann_json::nlohmann_json)
    target_include_directories(ZombiesModule PRIVATE ${CMAKE_SOURCE_DIR}/source ${CMAKE_SOURCE_DIR}/examples/zombies_module)
    # The game's interface (documents, style sheets, fonts) goes to Assets/ui next to the player, the campaign to Assets/game
    file(GLOB_RECURSE ZOMBIES_UI_FILES CONFIGURE_DEPENDS ${CMAKE_SOURCE_DIR}/examples/zombies_module/ui/* ${CMAKE_SOURCE_DIR}/examples/zombies_module/game/*)
    add_custom_target(ZombiesUI ALL
        COMMAND ${CMAKE_COMMAND} -E copy_directory ${CMAKE_SOURCE_DIR}/examples/zombies_module/ui ${CMAKE_RUNTIME_OUTPUT_DIRECTORY}/Assets/ui
        COMMAND ${CMAKE_COMMAND} -E copy_directory ${CMAKE_SOURCE_DIR}/examples/zombies_module/game ${CMAKE_RUNTIME_OUTPUT_DIRECTORY}/Assets/game
        DEPENDS ${ZOMBIES_UI_FILES})
    add_dependencies(ZombiesModule ZombiesUI)
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
    add_executable(CameraInputTests tests/CameraInputTests.cpp)
    target_include_directories(CameraInputTests PRIVATE ${CMAKE_SOURCE_DIR}/source)
    add_executable(ZombieRulesTests tests/ZombieRulesTests.cpp examples/zombies_module/GameRules.cpp game_genres/topdown/TopDownRules.cpp)
    target_link_libraries(ZombieRulesTests nlohmann_json::nlohmann_json)
    target_include_directories(ZombieRulesTests PRIVATE ${CMAKE_SOURCE_DIR}/source ${CMAKE_SOURCE_DIR}/examples/zombies_module)
    add_test(NAME ZombieRulesTests COMMAND ZombieRulesTests)
    add_test(NAME CameraInputTests COMMAND CameraInputTests)
endif()

# Brawler prototype: a second game on TopDown (one arena, hand to hand), to try the module. Its simulation is tested
# without a window; the module runs in PakalPlayer (PakalPlayer BrawlerModule.dll) with the Zombies example's models.
if(WIN32 AND EXISTS ${CMAKE_SOURCE_DIR}/examples/brawler_proto/BrawlerGame.cpp)
    add_library(BrawlerModule SHARED
        ${CMAKE_SOURCE_DIR}/examples/brawler_proto/BrawlerModule.cpp
        ${CMAKE_SOURCE_DIR}/examples/brawler_proto/BrawlerLevelState.cpp
        ${CMAKE_SOURCE_DIR}/examples/brawler_proto/BrawlerGame.cpp)
    target_link_libraries(BrawlerModule PakalTopDownView nlohmann_json::nlohmann_json)
    target_include_directories(BrawlerModule PRIVATE ${CMAKE_SOURCE_DIR}/source ${CMAKE_SOURCE_DIR}/examples/brawler_proto)
    # its rules go next to the player, in Assets/brawler
    file(GLOB_RECURSE BRAWLER_DATA_FILES CONFIGURE_DEPENDS ${CMAKE_SOURCE_DIR}/examples/brawler_proto/data/*)
    add_custom_target(BrawlerData ALL
        COMMAND ${CMAKE_COMMAND} -E copy_directory ${CMAKE_SOURCE_DIR}/examples/brawler_proto/data ${CMAKE_RUNTIME_OUTPUT_DIRECTORY}/Assets/brawler
        DEPENDS ${BRAWLER_DATA_FILES})
    add_dependencies(BrawlerModule BrawlerData)
    add_executable(BrawlerTests tests/BrawlerTests.cpp examples/brawler_proto/BrawlerGame.cpp)
    target_link_libraries(BrawlerTests PakalTopDown nlohmann_json::nlohmann_json)
    target_include_directories(BrawlerTests PRIVATE ${CMAKE_SOURCE_DIR}/source ${CMAKE_SOURCE_DIR}/examples/brawler_proto)
    add_test(NAME BrawlerTests COMMAND BrawlerTests ${CMAKE_SOURCE_DIR}/examples/brawler_proto/data/brawler.json)
endif()

# Dungeon prototype: a small Diablo-like on TopDown and its RPG kit (rooms made from a seed, abilities, loot, a boss). Its
# simulation is tested without a window; the module runs in PakalPlayer (PakalPlayer DungeonModule.dll). The dungeon's
# pieces come from the local Medieval Village pack (not in Git), copied next to the player when it is there.
if(WIN32 AND EXISTS ${CMAKE_SOURCE_DIR}/examples/dungeon_proto/DungeonGame.cpp)
    add_library(DungeonModule SHARED
        ${CMAKE_SOURCE_DIR}/examples/dungeon_proto/DungeonModule.cpp
        ${CMAKE_SOURCE_DIR}/examples/dungeon_proto/DungeonLevelState.cpp
        ${CMAKE_SOURCE_DIR}/examples/dungeon_proto/DungeonMenuState.cpp
        ${CMAKE_SOURCE_DIR}/examples/dungeon_proto/DungeonGame.cpp
        ${CMAKE_SOURCE_DIR}/examples/dungeon_proto/DungeonNet.cpp)
    target_link_libraries(DungeonModule PakalTopDownView PakalTopDownNet nlohmann_json::nlohmann_json)
    target_include_directories(DungeonModule PRIVATE ${CMAKE_SOURCE_DIR}/source ${CMAKE_SOURCE_DIR}/examples/dungeon_proto)
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
add_library(PakalLevelDoc STATIC ${CMAKE_SOURCE_DIR}/tools/level_doc/LevelDocument.cpp ${CMAKE_SOURCE_DIR}/tools/level_doc/LevelEditor.cpp)
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
add_executable(EventBusTests ${CMAKE_SOURCE_DIR}/tests/EventBusTests.cpp)
target_include_directories(EventBusTests PRIVATE ${CMAKE_SOURCE_DIR}/source)
add_test(NAME EventBusTests COMMAND EventBusTests)
add_executable(ActionTests ${CMAKE_SOURCE_DIR}/tests/ActionTests.cpp)
target_include_directories(ActionTests PRIVATE ${CMAKE_SOURCE_DIR}/source)
add_test(NAME ActionTests COMMAND ActionTests)
add_executable(AnimationMarksTests ${CMAKE_SOURCE_DIR}/tests/AnimationMarksTests.cpp)
target_include_directories(AnimationMarksTests PRIVATE ${CMAKE_SOURCE_DIR}/source)
add_test(NAME AnimationMarksTests COMMAND AnimationMarksTests)
add_executable(RpgKitTests ${CMAKE_SOURCE_DIR}/tests/RpgKitTests.cpp)
target_link_libraries(RpgKitTests PakalTopDown)
add_test(NAME RpgKitTests COMMAND RpgKitTests)

# Print configuration summary
message(STATUS "========== Pakal Engine Configuration ==========")
message(STATUS "C++ Standard: ${CMAKE_CXX_STANDARD}")
message(STATUS "Build Type: ${CMAKE_BUILD_TYPE}")
message(STATUS "SFML: ${SFML_FOUND}")
message(STATUS "Box2D: ${BOX2D_FOUND}")
message(STATUS "Lua: ${LUA_FOUND}")
message(STATUS "================================================")
