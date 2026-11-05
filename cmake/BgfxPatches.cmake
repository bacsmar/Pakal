# Our fixes to bgfx (patches/bgfx/*.patch), applied to the submodule when it does not have them yet. Included by the
# desktop and the Android builds; sets PAKAL_BGFX_PATCHED to the patches it applied in this configure.
find_package(Git QUIET)
set(PAKAL_BGFX_PATCHED "")
file(GLOB PAKAL_BGFX_PATCHES ${PAKAL_ROOT}/patches/bgfx/*.patch)
foreach(patch ${PAKAL_BGFX_PATCHES})
    execute_process(COMMAND ${GIT_EXECUTABLE} apply --reverse --check --ignore-whitespace ${patch}
        WORKING_DIRECTORY ${PAKAL_ROOT}/external/bgfx RESULT_VARIABLE applied OUTPUT_QUIET ERROR_QUIET)
    if(NOT applied EQUAL 0)
        execute_process(COMMAND ${GIT_EXECUTABLE} apply --ignore-whitespace ${patch}
            WORKING_DIRECTORY ${PAKAL_ROOT}/external/bgfx RESULT_VARIABLE failed)
        if(failed)
            message(FATAL_ERROR "bgfx: ${patch} does not apply (a newer bgfx may have it, or need it redone)")
        endif()
        message(STATUS "bgfx: applied ${patch}")
        list(APPEND PAKAL_BGFX_PATCHED ${patch})
    endif()
endforeach()
