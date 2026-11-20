# Games and genres never see the backends (docs/ARCHITECTURE.md, "The core and the backends"): a game module or a genre
# includes the engine's abstractions only, never a backend's classes (source/bgfx, source/sfml, the Box2D physics
# system) nor a library the engine is built on (bgfx, bx, bimg, Box2D, SFML, RmlUi, imgui).
#
# Run as a script: cmake -DPAKAL_ROOT=<the engine's folder> -P cmake/CheckBackendIncludes.cmake
# It reads the files of game_genres/, games/ and examples/ and fails when one includes a backend header and is not in
# cmake/backend_includes_pending.txt (the files still waiting to move to the abstractions). The list may only shrink: a
# file of the list that no longer includes one fails too, so its line is removed. A file of the list that this checkout
# does not have is skipped. A checkout's own games (not in Git) may list theirs in backend_includes_pending.local.txt.
cmake_minimum_required(VERSION 3.21)   # run with -P: without it the old policies apply, and if(... IN_LIST ...) fails

if(NOT PAKAL_ROOT)
    message(FATAL_ERROR "CheckBackendIncludes: PAKAL_ROOT is not set")
endif()
get_filename_component(PAKAL_ROOT "${PAKAL_ROOT}" ABSOLUTE)

# a backend's header: the third-party libraries, and the engine's backend classes. box2D/ (capital D) also holds the
# engine's 2D physics (PhysicsWorld2D, PhysicsBody2D, CharacterController2D, Box2DQueries), which games may use
set(BACKEND_INCLUDE "^[ \t]*#[ \t]*include[ \t]*[\"<](bgfx/|bx/|bimg/|box2d/|SFML/|sfml/|RmlUi/|imgui/|box2D/(Box2DPhysicsSystem|Box2DPhysicsListeners|B2DebugDrawBgfx|SpritePhysicsComponent_Box2D))")

set(pending "")
foreach(list_file backend_includes_pending.txt backend_includes_pending.local.txt)
    if(EXISTS ${PAKAL_ROOT}/cmake/${list_file})
        file(STRINGS ${PAKAL_ROOT}/cmake/${list_file} lines)
        foreach(line IN LISTS lines)
            string(STRIP "${line}" line)
            if(line AND NOT line MATCHES "^#")
                list(APPEND pending "${line}")
            endif()
        endforeach()
    endif()
endforeach()

set(sources "")
foreach(folder game_genres games examples)
    file(GLOB_RECURSE found RELATIVE ${PAKAL_ROOT} FOLLOW_SYMLINKS
        ${PAKAL_ROOT}/${folder}/*.h ${PAKAL_ROOT}/${folder}/*.hpp ${PAKAL_ROOT}/${folder}/*.inl
        ${PAKAL_ROOT}/${folder}/*.cpp ${PAKAL_ROOT}/${folder}/*.cc)
    list(APPEND sources ${found})
endforeach()
# the old engine and its SDKs (not Pakal's games)
list(FILTER sources EXCLUDE REGEX "^examples/old_games_to_be_ported/")

set(new_leaks "")
set(found_pending "")
foreach(source IN LISTS sources)
    file(STRINGS ${PAKAL_ROOT}/${source} includes REGEX "${BACKEND_INCLUDE}")
    if(includes)
        if(source IN_LIST pending)
            list(APPEND found_pending "${source}")
        else()
            list(GET includes 0 first)
            string(STRIP "${first}" first)
            list(APPEND new_leaks "  ${source}: ${first}")
        endif()
    endif()
endforeach()

set(cleaned "")
foreach(source IN LISTS pending)
    if(EXISTS ${PAKAL_ROOT}/${source} AND NOT source IN_LIST found_pending)
        list(APPEND cleaned "  ${source}")
    endif()
endforeach()

list(LENGTH found_pending waiting)
if(new_leaks OR cleaned)
    set(text "")
    if(new_leaks)
        list(JOIN new_leaks "\n" leaks_text)
        string(APPEND text "A game or genre includes a backend header (use the engine's abstraction; if it lacks what you need, add it there):\n${leaks_text}\n")
    endif()
    if(cleaned)
        list(JOIN cleaned "\n" cleaned_text)
        string(APPEND text "These files no longer include a backend header: remove them from cmake/backend_includes_pending*.txt:\n${cleaned_text}\n")
    endif()
    message(FATAL_ERROR "${text}")
endif()
message(STATUS "Backend includes: no new ones (${waiting} files still waiting to move to the abstractions)")
