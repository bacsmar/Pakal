# How a game builds on the engine: the engine's libraries by name (Pakal::Pakal, Pakal::TopDownView...) and
# pakal_add_game_module(). They are the names and the function a package of the engine (find_package(Pakal)) would
# give, so a game moved out of the engine's tree changes how it finds the engine, not how it builds.

# The engine's libraries by the names the games link (Pakal::Pakal, Pakal::TopDown, Pakal::TopDownView,
# Pakal::TopDownEditor, Pakal::TopDownNet, Pakal::Net, Pakal::Scripting,
# Pakal::LevelDoc, Pakal::Quests, Pakal::QuestsView). Called once every library is defined (the end of the root CMakeLists.txt); a name is resolved when
# the build is generated, so a game may link it before this runs. A library this build lacks gets no name.
function(pakal_name_targets)
    foreach(library Pakal PakalTopDown PakalTopDownView PakalTopDownEditor PakalTopDownNet PakalPlatformer PakalVersus
            PakalNet PakalScripting PakalLevelDoc PakalQuests PakalQuestsView)
        string(REGEX REPLACE "^Pakal(.+)$" "\\1" name ${library})
        if(TARGET ${library} AND NOT TARGET Pakal::${name})
            add_library(Pakal::${name} ALIAS ${library})
        endif()
    endforeach()
endfunction()

# A game module: the library PakalPlayer loads (PakalPlayer <target>.dll), next to the player, and its data copied under
# Assets/ next to it.
#
#   pakal_add_game_module(<target>
#       SOURCES <the module's sources>
#       LINKS <the engine's libraries it uses, by name (Pakal::TopDownView...), and its other libraries>
#       [INCLUDES <its own folders of headers>]
#       [DATA <source folder> <folder under Assets/> ...]      pairs: ui mygame/ui copies ui/ to Assets/mygame/ui
#       [DATA_TARGET <name>])                                    the target that copies them (<target>Data)
#
# It links Pakal::Pakal; the engine's headers come with it (a game names no folder of the engine). What the module needs
# beyond this (definitions, options) is added to <target> as to any library.
function(pakal_add_game_module target)
    cmake_parse_arguments(PARSE_ARGV 1 MODULE "" "DATA_TARGET" "SOURCES;LINKS;INCLUDES;DATA")
    if(NOT MODULE_SOURCES)
        message(FATAL_ERROR "pakal_add_game_module(${target}): no SOURCES")
    endif()
    add_library(${target} SHARED ${MODULE_SOURCES})
    set_target_properties(${target} PROPERTIES LIBRARY_OUTPUT_DIRECTORY ${CMAKE_RUNTIME_OUTPUT_DIRECTORY})
    target_link_libraries(${target} Pakal::Pakal ${MODULE_LINKS})
    if(MODULE_INCLUDES)
        target_include_directories(${target} PRIVATE ${MODULE_INCLUDES})
    endif()
    if(MODULE_DATA)
        list(LENGTH MODULE_DATA count)
        math(EXPR odd "${count} % 2")
        if(odd)
            message(FATAL_ERROR "pakal_add_game_module(${target}): DATA takes pairs (<source folder> <folder under Assets/>)")
        endif()
        if(NOT MODULE_DATA_TARGET)
            set(MODULE_DATA_TARGET ${target}Data)
        endif()
        set(commands "")
        set(files "")
        math(EXPR last "${count} - 1")
        foreach(i RANGE 0 ${last} 2)
            math(EXPR j "${i} + 1")
            list(GET MODULE_DATA ${i} from)
            list(GET MODULE_DATA ${j} to)
            get_filename_component(from "${from}" ABSOLUTE BASE_DIR ${CMAKE_CURRENT_SOURCE_DIR})
            list(APPEND commands COMMAND ${CMAKE_COMMAND} -E copy_directory ${from} ${CMAKE_RUNTIME_OUTPUT_DIRECTORY}/Assets/${to})
            file(GLOB_RECURSE found CONFIGURE_DEPENDS ${from}/*)
            list(APPEND files ${found})
        endforeach()
        add_custom_target(${MODULE_DATA_TARGET} ALL ${commands} DEPENDS ${files})
        add_dependencies(${target} ${MODULE_DATA_TARGET})
    endif()
endfunction()
