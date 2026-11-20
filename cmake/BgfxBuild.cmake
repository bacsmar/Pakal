# bgfx, bx, bimg and shaderc for the desktop, built from the submodules with bgfx's own tools (GENie, then MSBuild on
# Windows or make on Linux), the first time CMake configures, and again when the submodules or our patches change.
# Sets BGFX_DIR and BGFX_BUILD_DIR. A clone needs nothing done by hand: `cmake --preset win64` builds them (a few minutes,
# once). PAKAL_BGFX_AUTOBUILD=OFF skips it, for someone who builds bgfx themselves (docs/BUILDING.md).

option(PAKAL_BGFX_AUTOBUILD "Build bgfx, bx, bimg and shaderc from external/ when they are missing" ON)

set(PAKAL_ROOT ${CMAKE_SOURCE_DIR})
set(BGFX_DIR ${PAKAL_ROOT}/external/bgfx)
set(BX_DIR ${PAKAL_ROOT}/external/bx)

if(NOT EXISTS ${BGFX_DIR}/src/bgfx.cpp OR NOT EXISTS ${BX_DIR}/include/bx/bx.h OR NOT EXISTS ${PAKAL_ROOT}/external/bimg/include/bimg/bimg.h)
    message(FATAL_ERROR "bgfx, bx or bimg is missing from external/. Fetch the submodules first:\n"
        "    git submodule update --init --recursive")
endif()

# Our fixes to bgfx are applied by cmake/BgfxPatches.cmake (included before this file); PAKAL_BGFX_PATCHED lists the
# ones it applied in this configure, and the libraries are built again when it is not empty
find_package(Git QUIET)
file(GLOB PAKAL_BGFX_PATCHES ${PAKAL_ROOT}/patches/bgfx/*.patch)

if(WIN32)
    set(BGFX_BUILD_DIR ${BGFX_DIR}/.build/win64_vs2022/bin)
    set(PAKAL_BGFX_OUTPUTS bgfxRelease.lib bxRelease.lib bimgRelease.lib bimg_decodeRelease.lib shadercRelease.exe)
else()
    set(BGFX_BUILD_DIR ${BGFX_DIR}/.build/linux64_gcc/bin)
    set(PAKAL_BGFX_OUTPUTS libbgfxRelease.a libbxRelease.a libbimgRelease.a libbimg_decodeRelease.a shadercRelease)
endif()

set(PAKAL_BGFX_MISSING "")
foreach(output ${PAKAL_BGFX_OUTPUTS})
    if(NOT EXISTS ${BGFX_BUILD_DIR}/${output})
        list(APPEND PAKAL_BGFX_MISSING ${output})
    endif()
endforeach()

# What the libraries were built from: the three submodules' commits and our patches. A different one (a submodule
# updated, a patch added or changed) builds them again; a patch applied again to the same sources does not
set(PAKAL_BGFX_SOURCE "")
foreach(dir ${BGFX_DIR} ${BX_DIR} ${PAKAL_ROOT}/external/bimg)
    execute_process(COMMAND ${GIT_EXECUTABLE} rev-parse HEAD WORKING_DIRECTORY ${dir}
        OUTPUT_VARIABLE commit OUTPUT_STRIP_TRAILING_WHITESPACE ERROR_QUIET)
    string(APPEND PAKAL_BGFX_SOURCE "${commit};")
endforeach()
foreach(patch ${PAKAL_BGFX_PATCHES})
    file(SHA1 ${patch} hash)
    string(APPEND PAKAL_BGFX_SOURCE "${hash};")
endforeach()
# and how they are built (a change here builds them again: Linux's libraries were not position independent before)
string(APPEND PAKAL_BGFX_SOURCE "fpic;")
set(PAKAL_BGFX_STAMP ${BGFX_BUILD_DIR}/pakal_bgfx_source.txt)
set(PAKAL_BGFX_BUILT_FROM "")
if(EXISTS ${PAKAL_BGFX_STAMP})
    file(READ ${PAKAL_BGFX_STAMP} PAKAL_BGFX_BUILT_FROM)
endif()
if(PAKAL_BGFX_PATCHED OR NOT PAKAL_BGFX_BUILT_FROM STREQUAL PAKAL_BGFX_SOURCE)
    set(PAKAL_BGFX_STALE TRUE)
else()
    set(PAKAL_BGFX_STALE FALSE)
endif()

if((PAKAL_BGFX_MISSING OR PAKAL_BGFX_STALE) AND PAKAL_BGFX_AUTOBUILD)
    message(STATUS "bgfx: building bgfx, bx, bimg and shaderc from external/ (once; a few minutes)")
    if(WIN32)
        if(NOT MSVC)
            message(FATAL_ERROR "bgfx: the Windows build needs MSVC. Configure from a Developer shell for Visual Studio 2022 or 2026.")
        endif()
        # MSBuild from the same Visual Studio as the compiler
        get_filename_component(PAKAL_MSVC_BIN ${CMAKE_CXX_COMPILER} DIRECTORY)
        find_program(PAKAL_MSBUILD msbuild
            HINTS $ENV{VSINSTALLDIR}MSBuild/Current/Bin/amd64 $ENV{VSINSTALLDIR}MSBuild/Current/Bin
                  ${PAKAL_MSVC_BIN}/../../../../../../../MSBuild/Current/Bin/amd64)
        if(NOT PAKAL_MSBUILD)
            message(FATAL_ERROR "bgfx: msbuild was not found. Configure from a Developer shell (\"x64 Native Tools Command Prompt\").")
        endif()
        # the compiler's own toolset (19.50 and later: Visual Studio 2026's v145; a CMake older than the compiler may not
        # know MSVC_TOOLSET_VERSION for it)
        if(CMAKE_CXX_COMPILER_VERSION VERSION_GREATER_EQUAL 19.50)
            set(PAKAL_VS_TOOLSET v145)
        else()
            set(PAKAL_VS_TOOLSET v143)
        endif()
        # bgfx's third-party sources have deep paths: under a long folder they pass Windows' 260 characters
        string(LENGTH "${BGFX_DIR}" PAKAL_BGFX_DIR_LENGTH)
        if(PAKAL_BGFX_DIR_LENGTH GREATER 80)
            message(WARNING "bgfx: the clone is in a long path (${PAKAL_BGFX_DIR_LENGTH} characters to external/bgfx). "
                "shaderc's sources may pass Windows' 260-character limit: clone into a short folder (C:/dev/Pakal) "
                "or turn on long paths (git config core.longpaths true, and LongPathsEnabled in Windows).")
        endif()
        execute_process(COMMAND ${BX_DIR}/tools/bin/windows/genie.exe --with-tools --with-dynamic-runtime vs2022
            WORKING_DIRECTORY ${BGFX_DIR} RESULT_VARIABLE failed OUTPUT_QUIET)
        if(failed)
            message(FATAL_ERROR "bgfx: GENie could not generate the Visual Studio projects")
        endif()
        foreach(project bx bimg bimg_decode bgfx shaderc)
            message(STATUS "bgfx: building ${project} (${PAKAL_VS_TOOLSET}, Release x64)")
            execute_process(COMMAND ${PAKAL_MSBUILD} ${project}.vcxproj /m /nologo /v:minimal
                    /p:Configuration=Release /p:Platform=x64 /p:PlatformToolset=${PAKAL_VS_TOOLSET}
                WORKING_DIRECTORY ${BGFX_DIR}/.build/projects/vs2022
                RESULT_VARIABLE failed OUTPUT_VARIABLE log ERROR_VARIABLE log)
            if(failed)
                file(WRITE ${CMAKE_BINARY_DIR}/bgfx_build.log "${log}")
                message(FATAL_ERROR "bgfx: ${project} did not build. Its log: ${CMAKE_BINARY_DIR}/bgfx_build.log
"
                    "\"Cannot open source file\" there is a path past 260 characters: clone into a shorter folder.")
            endif()
        endforeach()
    else()
        include(ProcessorCount)
        ProcessorCount(PAKAL_CORES)
        if(PAKAL_CORES EQUAL 0)
            set(PAKAL_CORES 2)
        endif()
        # position independent: they go into libPakal.so, a shared library on Linux
        execute_process(COMMAND ${CMAKE_COMMAND} -E env CFLAGS=-fPIC CXXFLAGS=-fPIC make linux-gcc-release64 -j${PAKAL_CORES}
            WORKING_DIRECTORY ${BGFX_DIR} RESULT_VARIABLE failed OUTPUT_VARIABLE log ERROR_VARIABLE log)
        if(failed)
            file(WRITE ${CMAKE_BINARY_DIR}/bgfx_build.log "${log}")
            message(FATAL_ERROR "bgfx: make linux-gcc-release64 failed. Its log: ${CMAKE_BINARY_DIR}/bgfx_build.log")
        endif()
    endif()
    set(PAKAL_BGFX_MISSING "")
    foreach(output ${PAKAL_BGFX_OUTPUTS})
        if(NOT EXISTS ${BGFX_BUILD_DIR}/${output})
            list(APPEND PAKAL_BGFX_MISSING ${output})
        endif()
    endforeach()
    if(NOT PAKAL_BGFX_MISSING)
        file(WRITE ${PAKAL_BGFX_STAMP} "${PAKAL_BGFX_SOURCE}")
    endif()
endif()

if(PAKAL_BGFX_PATCHED AND NOT PAKAL_BGFX_AUTOBUILD)
    message(WARNING "bgfx was just patched: rebuild its libraries (docs/BUILDING.md, step 2) before building Pakal")
endif()

if(PAKAL_BGFX_MISSING)
    message(FATAL_ERROR "bgfx: ${PAKAL_BGFX_MISSING} missing from ${BGFX_BUILD_DIR}.\n"
        "Leave PAKAL_BGFX_AUTOBUILD on to build them, or build bgfx by hand (docs/BUILDING.md).")
endif()
message(STATUS "bgfx: ${BGFX_BUILD_DIR}")
