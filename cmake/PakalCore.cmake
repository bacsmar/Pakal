# PakalCore: the engine's core alone, built with every build. No bgfx, no Box2D, no SFML, no Assimp, no RmlUi: the dummy
# systems take the backends' place (PAKAL_CORE_ONLY in Config.h). It only sees source/ (it is defined before the include
# folders the rest of the project shares), so a core file that reaches for a backend breaks this target. CoreTests starts
# the engine with it, runs its systems a moment and ends it.

file(GLOB_RECURSE PAKAL_CORE_SOURCES CONFIGURE_DEPENDS
    "${CMAKE_SOURCE_DIR}/source/*.cpp"
    "${CMAKE_SOURCE_DIR}/source/*.h")
# the backends and what is built on them
list(FILTER PAKAL_CORE_SOURCES EXCLUDE REGEX ".*/source/(bgfx|box2D|sfml|android|win32|unix|mcp|net|scripting|gameplay)/.*")
# DynamicMesh draws through a graphics backend; the examples of the persist submodule are not the engine
list(FILTER PAKAL_CORE_SOURCES EXCLUDE REGEX ".*/source/components/DynamicMesh/.*")
list(FILTER PAKAL_CORE_SOURCES EXCLUDE REGEX ".*/examples/.*")
# the desktop player is the full engine's (it starts bgfx and the MCP server)
list(FILTER PAKAL_CORE_SOURCES EXCLUDE REGEX ".*/source/PakalPlayerMain\\.cpp$")
# pugixml is compiled once, through the unity unit source/CompileUnit_1.cpp
list(FILTER PAKAL_CORE_SOURCES EXCLUDE REGEX ".*/persist/pugixml/pugixml\\.cpp$")
if(WIN32)
    list(APPEND PAKAL_CORE_SOURCES ${CMAKE_SOURCE_DIR}/source/win32/ClockImpl.cpp)
else()
    list(APPEND PAKAL_CORE_SOURCES ${CMAKE_SOURCE_DIR}/source/unix/ClockImpl.cpp)
endif()

add_library(PakalCore STATIC ${PAKAL_CORE_SOURCES})
target_compile_definitions(PakalCore PUBLIC PAKAL_CORE_ONLY PAKAL_STATIC_LIB PRIVATE pugi=pugi_pakal)
target_include_directories(PakalCore PUBLIC ${CMAKE_BINARY_DIR} ${CMAKE_SOURCE_DIR}/source)
# nlohmann-json (header only): the telemetry's records
find_package(nlohmann_json CONFIG REQUIRED)
target_link_libraries(PakalCore PRIVATE nlohmann_json::nlohmann_json)
if(WIN32)
    target_link_libraries(PakalCore PRIVATE winmm psapi)
else()
    target_link_libraries(PakalCore PRIVATE pthread dl)
endif()
if(MSVC)
    target_compile_options(PakalCore PRIVATE /W4 /Zc:__cplusplus /Zc:preprocessor)
endif()

add_executable(CoreTests ${CMAKE_SOURCE_DIR}/tests/CoreTests.cpp)
target_link_libraries(CoreTests PRIVATE PakalCore)
add_test(NAME CoreTests COMMAND CoreTests)
