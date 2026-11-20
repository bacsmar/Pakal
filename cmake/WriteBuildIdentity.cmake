# A desktop build's identity (docs/BUILD_IDENTITY.md): build.json next to the programs, written at every build, so the engine
# logs which copy runs and draws its label. A developer's copy: channel "dev", the commit and whether it has uncommitted
# changes. Run as a script: cmake -DROOT=<repository> -DOUT=<bin>/build.json -P WriteBuildIdentity.cmake
find_package(Git QUIET)
set(commit "")
set(branch "")
set(dirty false)
if(GIT_EXECUTABLE)
    execute_process(COMMAND ${GIT_EXECUTABLE} rev-parse --short HEAD WORKING_DIRECTORY ${ROOT}
        OUTPUT_VARIABLE commit OUTPUT_STRIP_TRAILING_WHITESPACE ERROR_QUIET)
    execute_process(COMMAND ${GIT_EXECUTABLE} rev-parse --abbrev-ref HEAD WORKING_DIRECTORY ${ROOT}
        OUTPUT_VARIABLE branch OUTPUT_STRIP_TRAILING_WHITESPACE ERROR_QUIET)
    execute_process(COMMAND ${GIT_EXECUTABLE} status --porcelain --untracked-files=no --ignore-submodules=all WORKING_DIRECTORY ${ROOT}
        OUTPUT_VARIABLE changes OUTPUT_STRIP_TRAILING_WHITESPACE ERROR_QUIET)
    if(changes)
        set(dirty true)
    endif()
endif()
set(text "{\n \"channel\": \"dev\",\n \"commit\": \"${commit}\",\n \"branch\": \"${branch}\",\n \"dirty\": ${dirty}\n}\n")
# written only when it changed, so nothing that depends on it builds again
set(old "")
if(EXISTS ${OUT})
    file(READ ${OUT} old)
endif()
if(NOT old STREQUAL text)
    file(WRITE ${OUT} "${text}")
endif()
