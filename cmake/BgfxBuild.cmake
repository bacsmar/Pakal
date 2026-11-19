# bgfx, bx, bimg and shaderc for the desktop, built from the submodules with bgfx's own tools (GENie, then MSBuild on
# Windows or make on Linux), the first time CMake configures, or again when one of our patches had to be applied.
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

# Our fixes to bgfx (patches/bgfx/*.patch), applied when the submodule does not have them yet
find_package(Git QUIET)
set(PAKAL_BGFX_PATCHED "")
file(GLOB PAKAL_BGFX_PATCHES ${PAKAL_ROOT}/patches/bgfx/*.patch)
foreach(patch ${PAKAL_BGFX_PATCHES})
    execute_process(COMMAND ${GIT_EXECUTABLE} apply --reverse --check --ignore-whitespace ${patch}
        WORKING_DIRECTORY ${BGFX_DIR} RESULT_VARIABLE applied OUTPUT_QUIET ERROR_QUIET)
    if(NOT applied EQUAL 0)
        execute_process(COMMAND ${GIT_EXECUTABLE} apply --ignore-whitespace ${patch}
            WORKING_DIRECTORY ${BGFX_DIR} RESULT_VARIABLE failed)
        if(failed)
            message(FATAL_ERROR "bgfx: ${patch} does not apply (a newer bgfx may have it, or need it redone)")
        endif()
        message(STATUS "bgfx: applied ${patch}")
        list(APPEND PAKAL_BGFX_PATCHED ${patch})
    endif()
endforeach()

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

if((PAKAL_BGFX_MISSING OR PAKAL_BGFX_PATCHED) AND PAKAL_BGFX_AUTOBUILD)
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
        if(MSVC_TOOLSET_VERSION GREATER_EQUAL 145)
            set(PAKAL_VS_TOOLSET v145)
        else()
            set(PAKAL_VS_TOOLSET v143)
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
                message(FATAL_ERROR "bgfx: ${project} did not build. Its log: ${CMAKE_BINARY_DIR}/bgfx_build.log")
            endif()
        endforeach()
    else()
        include(ProcessorCount)
        ProcessorCount(PAKAL_CORES)
        if(PAKAL_CORES EQUAL 0)
            set(PAKAL_CORES 2)
        endif()
        execute_process(COMMAND make linux-gcc-release64 -j${PAKAL_CORES}
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
endif()

if(PAKAL_BGFX_MISSING)
    message(FATAL_ERROR "bgfx: ${PAKAL_BGFX_MISSING} missing from ${BGFX_BUILD_DIR}.\n"
        "Leave PAKAL_BGFX_AUTOBUILD on to build them, or build bgfx by hand (docs/BUILDING.md).")
endif()
message(STATUS "bgfx: ${BGFX_BUILD_DIR}")
