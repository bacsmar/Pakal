# Android build of Pakal and its games: for each game one shared library (libpakal_<game>.so: the engine, the game's
# module and android/main_android.cpp) that its APK's NativeActivity loads. Everything is compiled from source for
# arm64-v8a with the NDK: bx/bimg/bgfx (OpenGL ES 3), Box2D, the engine, the games and, for the games that play online,
# ENet. Dependencies from vcpkg (arm64-android): assimp, lua,
# nlohmann-json. The shaders are compiled for ESSL with the shaderc that the Windows build of bgfx made.
#
# Use android/build_apk.py: it configures this project, builds it and packs the APKs.
cmake_minimum_required(VERSION 3.22)
project(PakalAndroid LANGUAGES C CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_POSITION_INDEPENDENT_CODE ON)

get_filename_component(PAKAL_ROOT ${CMAKE_CURRENT_LIST_DIR}/.. ABSOLUTE)
set(PAKAL_VCPKG_ANDROID ${CMAKE_BINARY_DIR}/vcpkg_installed/arm64-android CACHE PATH "vcpkg arm64-android install tree")
set(PAKAL_BOX2D_SRC "$ENV{USERPROFILE}/vcpkg/buildtrees/box2d/src/64c16f51bc-7b27809cb5.clean" CACHE PATH "Box2D 2.4.1 source (the one vcpkg extracted)")
set(PAKAL_SHADERC ${PAKAL_ROOT}/external/bgfx/.build/win64_vs2022/bin/shadercRelease.exe CACHE FILEPATH "shaderc of bgfx (host tool)")

set(BX_DIR ${PAKAL_ROOT}/external/bx)
set(BIMG_DIR ${PAKAL_ROOT}/external/bimg)
set(BGFX_DIR ${PAKAL_ROOT}/external/bgfx)

# ---------------------------------------------------------------------------------------------------
# bx, bimg, bgfx
# ---------------------------------------------------------------------------------------------------
set(PAKAL_BGFX_DEBUG 0 CACHE STRING "1: bgfx traces (shader link errors, ...) and checks")
set(BGFX_DEFINES
    BX_CONFIG_DEBUG=${PAKAL_BGFX_DEBUG}
    BGFX_CONFIG_DEBUG=${PAKAL_BGFX_DEBUG}
    BGFX_CONFIG_RENDERER_OPENGLES=30
    __STDC_LIMIT_MACROS __STDC_FORMAT_MACROS __STDC_CONSTANT_MACROS)

add_library(bx STATIC ${BX_DIR}/src/amalgamated.cpp)
target_include_directories(bx PUBLIC ${BX_DIR}/include ${BX_DIR}/3rdparty)
target_compile_definitions(bx PUBLIC ${BGFX_DEFINES})

file(GLOB ASTC_SOURCES ${BIMG_DIR}/3rdparty/astc-encoder/source/*.cpp)
add_library(bimg STATIC ${BIMG_DIR}/src/image.cpp ${BIMG_DIR}/src/image_gnf.cpp ${ASTC_SOURCES})
target_include_directories(bimg PUBLIC ${BIMG_DIR}/include PRIVATE ${BIMG_DIR}/3rdparty/astc-encoder/include)
target_link_libraries(bimg PUBLIC bx)

add_library(bimg_decode STATIC ${BIMG_DIR}/src/image_decode.cpp ${BIMG_DIR}/3rdparty/tinyexr/deps/miniz/miniz.c)
target_include_directories(bimg_decode PRIVATE ${BIMG_DIR}/3rdparty ${BIMG_DIR}/3rdparty/tinyexr/deps/miniz)
target_link_libraries(bimg_decode PUBLIC bimg)

add_library(bgfx STATIC ${BGFX_DIR}/src/amalgamated.cpp)
target_include_directories(bgfx PUBLIC ${BGFX_DIR}/include PRIVATE ${BGFX_DIR}/3rdparty ${BGFX_DIR}/3rdparty/khronos)
target_link_libraries(bgfx PUBLIC bx bimg)

# ---------------------------------------------------------------------------------------------------
# Box2D 2.4.1
# ---------------------------------------------------------------------------------------------------
file(GLOB_RECURSE BOX2D_SOURCES ${PAKAL_BOX2D_SRC}/src/*.cpp)
add_library(box2d STATIC ${BOX2D_SOURCES})
target_include_directories(box2d PUBLIC ${PAKAL_BOX2D_SRC}/include PRIVATE ${PAKAL_BOX2D_SRC}/src)

# ---------------------------------------------------------------------------------------------------
# The engine (without SFML, the Windows clock, the MCP server and the desktop player)
# ---------------------------------------------------------------------------------------------------
file(GLOB_RECURSE PAKAL_SOURCES
    ${PAKAL_ROOT}/source/*.cpp
    ${PAKAL_ROOT}/source/components/*.cpp
    ${PAKAL_ROOT}/source/resources/*.cpp)
file(GLOB_RECURSE PAKAL_POLY2TRI_SOURCES ${PAKAL_ROOT}/source/components/DynamicMesh/poly2tri/*.cc)
file(GLOB PAKAL_MESHOPT_SOURCES ${BGFX_DIR}/3rdparty/meshoptimizer/src/*.cpp)
set(PAKAL_IMGUI_SOURCES
    ${BGFX_DIR}/3rdparty/dear-imgui/imgui.cpp ${BGFX_DIR}/3rdparty/dear-imgui/imgui_draw.cpp
    ${BGFX_DIR}/3rdparty/dear-imgui/imgui_tables.cpp ${BGFX_DIR}/3rdparty/dear-imgui/imgui_widgets.cpp
    ${BGFX_DIR}/examples/common/imgui/imgui.cpp)
list(APPEND PAKAL_SOURCES ${PAKAL_POLY2TRI_SOURCES} ${BGFX_DIR}/examples/common/debugdraw/debugdraw.cpp ${PAKAL_MESHOPT_SOURCES}
    ${PAKAL_IMGUI_SOURCES})
list(FILTER PAKAL_SOURCES EXCLUDE REGEX ".*/source/(android|win32|unix|sfml|mcp)/.*")
list(APPEND PAKAL_SOURCES ${PAKAL_ROOT}/source/unix/ClockImpl.cpp)
list(FILTER PAKAL_SOURCES EXCLUDE REGEX ".*/source/PakalPlayerMain\\.cpp$")
list(FILTER PAKAL_SOURCES EXCLUDE REGEX ".*/persist/pugixml/pugixml\\.cpp$")
# the network (ENet, source/net, game_genres/topdown/net) goes only into the games that play online: a library of its own
list(FILTER PAKAL_SOURCES EXCLUDE REGEX ".*/source/net/.*")

# the TopDown genre (game_genres/topdown) the games are built on
file(GLOB_RECURSE PAKAL_GENRE_SOURCES ${PAKAL_ROOT}/game_genres/*.cpp)
set(PAKAL_GENRE_NET_SOURCES ${PAKAL_GENRE_SOURCES})
list(FILTER PAKAL_GENRE_SOURCES EXCLUDE REGEX ".*/topdown/net/.*")
list(FILTER PAKAL_GENRE_NET_SOURCES INCLUDE REGEX ".*/topdown/net/.*")

set(PAKAL_DEFINITIONS PAKAL_STATIC_LIB PAKAL_NO_SFML PAKAL_STATIC_MODULE RMLUI_STATIC_LIB pugi=pugi_pakal p2t=p2t_pakal)
set(PAKAL_INCLUDES
    ${PAKAL_ROOT}/source ${PAKAL_ROOT}/game_genres ${PAKAL_ROOT}/tools ${PAKAL_ROOT}/tools/level_doc ${CMAKE_CURRENT_LIST_DIR}
    ${BGFX_DIR}/examples/common ${BGFX_DIR}/3rdparty ${BGFX_DIR}/3rdparty/native_app_glue ${BGFX_DIR}/3rdparty/meshoptimizer/src
    ${PAKAL_VCPKG_ANDROID}/include)

# Assimp and what it needs
set(ASSIMP_LIBS
    ${PAKAL_VCPKG_ANDROID}/lib/libassimp.a ${PAKAL_VCPKG_ANDROID}/lib/libpugixml.a
    ${PAKAL_VCPKG_ANDROID}/lib/libkubazip.a ${PAKAL_VCPKG_ANDROID}/lib/libminizip.a
    ${PAKAL_VCPKG_ANDROID}/lib/libpoly2tri.a ${PAKAL_VCPKG_ANDROID}/lib/libpolyclipping.a
    ${PAKAL_VCPKG_ANDROID}/lib/libz.a)

# RmlUi (the game's interface) and FreeType with what it reads fonts with
set(RMLUI_LIBS
    ${PAKAL_VCPKG_ANDROID}/lib/librmlui.a ${PAKAL_VCPKG_ANDROID}/lib/libfreetype.a ${PAKAL_VCPKG_ANDROID}/lib/libpng16.a
    ${PAKAL_VCPKG_ANDROID}/lib/libbz2.a ${PAKAL_VCPKG_ANDROID}/lib/libbrotlidec.a ${PAKAL_VCPKG_ANDROID}/lib/libbrotlicommon.a)

# The engine, the genre, the level document library and the Android entry point, compiled once for every game. An object
# library: each game's library takes all of it, as the one library of before did (the linker drops nothing).
add_library(pakal_core OBJECT
    ${PAKAL_SOURCES} ${PAKAL_GENRE_SOURCES}
    ${PAKAL_ROOT}/tools/level_doc/LevelDocument.cpp
    ${PAKAL_ROOT}/tools/level_doc/LevelEditor.cpp
    ${CMAKE_CURRENT_LIST_DIR}/main_android.cpp
    ${CMAKE_CURRENT_LIST_DIR}/SoundManagerAAudio.cpp
    ${BGFX_DIR}/3rdparty/native_app_glue/android_native_app_glue.c)
target_compile_definitions(pakal_core PRIVATE ${PAKAL_DEFINITIONS})
target_include_directories(pakal_core PRIVATE ${PAKAL_INCLUDES})
target_link_libraries(pakal_core PRIVATE bgfx bimg_decode bimg bx box2d)

# The network: ENet (external/enet) and the engine's and the genre's code over it
file(GLOB ENET_SOURCES ${PAKAL_ROOT}/external/enet/*.c)
add_library(pakal_net STATIC ${ENET_SOURCES}
    ${PAKAL_ROOT}/source/net/NetHost.cpp ${PAKAL_ROOT}/source/net/LanDiscovery.cpp ${PAKAL_GENRE_NET_SOURCES})
target_compile_definitions(pakal_net PRIVATE ${PAKAL_DEFINITIONS}
    HAS_GETADDRINFO HAS_GETNAMEINFO HAS_INET_PTON HAS_INET_NTOP HAS_MSGHDR_FLAGS HAS_SOCKLEN_T HAS_FCNTL HAS_POLL)
target_include_directories(pakal_net PUBLIC ${PAKAL_ROOT}/external/enet/include PRIVATE ${PAKAL_INCLUDES})
target_link_libraries(pakal_net PRIVATE box2d)

# A game: the engine and its module in one shared library (lib<name>.so), the one its APK's NativeActivity loads
function(pakal_game name module_dir)
    add_library(${name} SHARED $<TARGET_OBJECTS:pakal_core> ${ARGN})
    target_compile_definitions(${name} PRIVATE ${PAKAL_DEFINITIONS})
    target_include_directories(${name} PRIVATE ${PAKAL_INCLUDES} ${module_dir})
    target_link_libraries(${name} PRIVATE
        bgfx bimg_decode bimg bx box2d
        ${PAKAL_VCPKG_ANDROID}/lib/liblua.a ${RMLUI_LIBS} ${ASSIMP_LIBS}
        android log aaudio EGL GLESv3 dl)
    # native_app_glue's entry point must survive the linker
    target_link_options(${name} PRIVATE -Wl,-u,ANativeActivity_onCreate -Wl,-z,max-page-size=16384)
endfunction()

# Zombies (examples/zombies_module)
set(ZOMBIES_DIR ${PAKAL_ROOT}/examples/zombies_module)
pakal_game(pakal_zombies ${ZOMBIES_DIR}
    ${ZOMBIES_DIR}/ZombiesModule.cpp ${ZOMBIES_DIR}/ZombiesLevelState.cpp ${ZOMBIES_DIR}/ZombieScript.cpp
    ${ZOMBIES_DIR}/AutoplayBot.cpp ${ZOMBIES_DIR}/WorldOverlays.cpp ${ZOMBIES_DIR}/ZombiesDevPanel.cpp ${ZOMBIES_DIR}/ZombiesHud.cpp
    ${ZOMBIES_DIR}/ZombiesScripting.cpp ${ZOMBIES_DIR}/ZombiesLoading.cpp ${ZOMBIES_DIR}/LevelLook.cpp ${ZOMBIES_DIR}/CharacterViewer.cpp
    ${ZOMBIES_DIR}/ZombiesGame.cpp ${ZOMBIES_DIR}/Components.cpp ${ZOMBIES_DIR}/GameRules.cpp
    ${ZOMBIES_DIR}/Meta.cpp ${ZOMBIES_DIR}/MenuState.cpp)

# The brawler prototype (examples/brawler_proto)
set(BRAWLER_DIR ${PAKAL_ROOT}/examples/brawler_proto)
pakal_game(pakal_brawler ${BRAWLER_DIR}
    ${BRAWLER_DIR}/BrawlerModule.cpp ${BRAWLER_DIR}/BrawlerLevelState.cpp ${BRAWLER_DIR}/BrawlerGame.cpp)

# The dungeon prototype (examples/dungeon_proto), which plays over the network too
set(DUNGEON_DIR ${PAKAL_ROOT}/examples/dungeon_proto)
pakal_game(pakal_dungeon ${DUNGEON_DIR}
    ${DUNGEON_DIR}/DungeonModule.cpp ${DUNGEON_DIR}/DungeonLevelState.cpp ${DUNGEON_DIR}/DungeonMenuState.cpp
    ${DUNGEON_DIR}/DungeonGame.cpp ${DUNGEON_DIR}/DungeonNet.cpp)
target_link_libraries(pakal_dungeon PRIVATE pakal_net)

# ---------------------------------------------------------------------------------------------------
# Shaders for OpenGL ES 3 (ESSL 300), by the same list as the Windows build
# ---------------------------------------------------------------------------------------------------
set(PAKAL_SHADER_SRC ${PAKAL_ROOT}/source/bgfx/shaders)
set(PAKAL_SHADER_DIR ${CMAKE_BINARY_DIR}/apk_assets/Assets/shaders/essl)
function(pakal_compile_shader name type varying)
    set(out ${PAKAL_SHADER_DIR}/${name}.bin)
    add_custom_command(OUTPUT ${out}
        COMMAND ${CMAKE_COMMAND} -E make_directory ${PAKAL_SHADER_DIR}
        COMMAND ${PAKAL_SHADERC}
            -f ${PAKAL_SHADER_SRC}/${name}.sc -o ${out}
            --type ${type} --platform android -p 300_es
            --varyingdef ${PAKAL_SHADER_SRC}/${varying}
            -i ${BGFX_DIR}/src -i ${PAKAL_SHADER_SRC}
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
add_custom_target(PakalShadersEssl ALL DEPENDS ${PAKAL_SHADER_OUTPUTS})
