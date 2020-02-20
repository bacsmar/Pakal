cmake_minimum_required(VERSION 3.13.0 FATAL_ERROR)

set(CMAKE_SYSTEM_VERSION 10.0.17134.0 CACHE STRING "" FORCE)

project(Pakal_II CXX)

################################################################################
# Set target arch type if empty. Visual studio solution generator provides it.
################################################################################
if(NOT CMAKE_VS_PLATFORM_NAME)
    set(CMAKE_VS_PLATFORM_NAME "Win32")
endif()
message("${CMAKE_VS_PLATFORM_NAME} architecture in use")

if(NOT ("${CMAKE_VS_PLATFORM_NAME}" STREQUAL "Win32"))
    message(FATAL_ERROR "${CMAKE_VS_PLATFORM_NAME} arch is not supported!")
endif()

################################################################################
# Global configuration types
################################################################################
set(CMAKE_CONFIGURATION_TYPES
    "Debug"
    "Debug_Static"
    "Release"
    "release_static"
    CACHE STRING "" FORCE
)

################################################################################
# Global compiler options
################################################################################

# specify the C++ standard
set(CMAKE_CXX_STANDARD 14)
set(CMAKE_CXX_STANDARD_REQUIRED True)
set(CMAKE_CXX_FLAGS "-std=gnu++14 -static-libstdc++ -pthread -fexceptions -fno-rtti  -O2")
set(CMAKE_CXX_FLAGS_DEBUG_STATIC "-DPAKAL_STATIC_LIB -DPERSIST_STATIC_LIB")

if(MSVC)
    # remove default flags provided with CMake for MSVC
    set(CMAKE_CXX_FLAGS "")
    set(CMAKE_CXX_FLAGS_DEBUG "")
    set(CMAKE_CXX_FLAGS_DEBUG_STATIC "")
    set(CMAKE_CXX_FLAGS_RELEASE "")
    set(CMAKE_CXX_FLAGS_RELEASE_STATIC "")
endif()

################################################################################
# Global linker options
################################################################################
if(MSVC)
    # remove default flags provided with CMake for MSVC
    set(CMAKE_EXE_LINKER_FLAGS "")
    set(CMAKE_MODULE_LINKER_FLAGS "")
    set(CMAKE_SHARED_LINKER_FLAGS "")
    set(CMAKE_STATIC_LINKER_FLAGS "")
    set(CMAKE_EXE_LINKER_FLAGS_DEBUG "${CMAKE_EXE_LINKER_FLAGS}")
    set(CMAKE_MODULE_LINKER_FLAGS_DEBUG "${CMAKE_MODULE_LINKER_FLAGS}")
    set(CMAKE_SHARED_LINKER_FLAGS_DEBUG "${CMAKE_SHARED_LINKER_FLAGS}")
    set(CMAKE_STATIC_LINKER_FLAGS_DEBUG "${CMAKE_STATIC_LINKER_FLAGS}")
    set(CMAKE_EXE_LINKER_FLAGS_DEBUG_STATIC "${CMAKE_EXE_LINKER_FLAGS}")
    set(CMAKE_MODULE_LINKER_FLAGS_DEBUG_STATIC "${CMAKE_MODULE_LINKER_FLAGS}")
    set(CMAKE_SHARED_LINKER_FLAGS_DEBUG_STATIC "${CMAKE_SHARED_LINKER_FLAGS}")
    set(CMAKE_STATIC_LINKER_FLAGS_DEBUG_STATIC "${CMAKE_STATIC_LINKER_FLAGS}")
    set(CMAKE_EXE_LINKER_FLAGS_RELEASE "${CMAKE_EXE_LINKER_FLAGS}")
    set(CMAKE_MODULE_LINKER_FLAGS_RELEASE "${CMAKE_MODULE_LINKER_FLAGS}")
    set(CMAKE_SHARED_LINKER_FLAGS_RELEASE "${CMAKE_SHARED_LINKER_FLAGS}")
    set(CMAKE_STATIC_LINKER_FLAGS_RELEASE "${CMAKE_STATIC_LINKER_FLAGS}")
    set(CMAKE_EXE_LINKER_FLAGS_RELEASE_STATIC "${CMAKE_EXE_LINKER_FLAGS}")
    set(CMAKE_MODULE_LINKER_FLAGS_RELEASE_STATIC "${CMAKE_MODULE_LINKER_FLAGS}")
    set(CMAKE_SHARED_LINKER_FLAGS_RELEASE_STATIC "${CMAKE_SHARED_LINKER_FLAGS}")
    set(CMAKE_STATIC_LINKER_FLAGS_RELEASE_STATIC "${CMAKE_STATIC_LINKER_FLAGS}")
endif()

message("${CMAKE_CXX_FLAGS} FLAGS in use")

################################################################################
# Nuget packages function stub.
################################################################################
function(use_package TARGET PACKAGE VERSION)
    message(WARNING "No implementation of use_package. Create yours. "
                    "Package \"${PACKAGE}\" with version \"${VERSION}\" "
                    "for target \"${TARGET}\" is ignored!")
endfunction()

################################################################################
# Common utils
################################################################################
include(CMake/Utils.cmake)

################################################################################
# Additional Global Settings(add specific info there)
################################################################################
include(CMake/GlobalSettingsInclude.cmake OPTIONAL)

################################################################################
# Use solution folders feature
################################################################################
set_property(GLOBAL PROPERTY USE_FOLDERS ON)

################################################################################
# Sub-projects
################################################################################



























project(Pakal_II CXX)

################################################################################
# Source groups
################################################################################
set(no_group_source_files
    "../source/CompileUnit_1.cpp"
    "../source/Engine.cpp"
    "../source/Engine.h"
    "../source/EngineComponents.cpp"
    "../source/EngineSettings.cpp"
    "../source/SingletonHolder.h"
)
source_group("" FILES ${no_group_source_files})

set(Components
    "../source/components/AutomataComponent.cpp"
    "../source/components/AutomataComponent.h"
    "../source/components/CharacterComponent.cpp"
    "../source/components/CharacterComponent.h"
    "../source/components/PhysicComponent.h"
)
source_group("Components" FILES ${Components})

set(Components__GFX
    "../source/components/CameraComponent.h"
    "../source/components/GridComponent.h"
    "../source/components/TerrainComponent.h"
)
source_group("Components\\GFX" FILES ${Components__GFX})

set(Components__GFX__DynamicMesh
    "../source/components/DynamicMesh/DynamicMesh.cpp"
    "../source/components/DynamicMesh/DynamicMesh.h"
    "../source/components/DynamicMesh/DynamicMeshComponent.h"
    "../source/components/DynamicMesh/MeshBuilder.cpp"
    "../source/components/DynamicMesh/MeshBuilder.h"
    "../source/components/DynamicMesh/MeshFns.h"
    "../source/components/DynamicMesh/UVMapping.cpp"
    "../source/components/DynamicMesh/UVMapping.h"
    "../source/components/DynamicMesh/VertexInfo.cpp"
    "../source/components/DynamicMesh/VertexInfo.h"
)
source_group("Components\\GFX\\DynamicMesh" FILES ${Components__GFX__DynamicMesh})

set(Components__GFX__DynamicMesh__poly2tri
    "../source/components/DynamicMesh/poly2tri/common/shapes.cc"
    "../source/components/DynamicMesh/poly2tri/common/shapes.h"
    "../source/components/DynamicMesh/poly2tri/common/utils.h"
    "../source/components/DynamicMesh/poly2tri/poly2tri.h"
    "../source/components/DynamicMesh/poly2tri/sweep/advancing_front.cc"
    "../source/components/DynamicMesh/poly2tri/sweep/advancing_front.h"
    "../source/components/DynamicMesh/poly2tri/sweep/cdt.cc"
    "../source/components/DynamicMesh/poly2tri/sweep/cdt.h"
    "../source/components/DynamicMesh/poly2tri/sweep/sweep.cc"
    "../source/components/DynamicMesh/poly2tri/sweep/sweep.h"
    "../source/components/DynamicMesh/poly2tri/sweep/sweep_context.cc"
    "../source/components/DynamicMesh/poly2tri/sweep/sweep_context.h"
)
source_group("Components\\GFX\\DynamicMesh\\poly2tri" FILES ${Components__GFX__DynamicMesh__poly2tri})

set(Components__GFX__Mesh
    "../source/components/AnimatedMeshComponent.h"
    "../source/components/MeshComponent.h"
)
source_group("Components\\GFX\\Mesh" FILES ${Components__GFX__Mesh})

set(Components__GFX__Sprite
    "../source/components/Sprite.cpp"
    "../source/components/Sprite.h"
    "../source/components/SpriteComponent.h"
)
source_group("Components\\GFX\\Sprite" FILES ${Components__GFX__Sprite})

set(Components__GFX__Tiles
    "../source/components/TileMapComponent.h"
)
source_group("Components\\GFX\\Tiles" FILES ${Components__GFX__Tiles})

set(Components__Input
    "../source/components/GamepadComponent.cpp"
    "../source/components/GamepadComponent.h"
    "../source/components/ICharacterHandler.h"
    "../source/components/InputBinder.h"
    "../source/components/InputHandlerComponent.cpp"
    "../source/components/InputHandlerComponent.h"
)
source_group("Components\\Input" FILES ${Components__Input})

set(Components__Music
    "../source/components/MusicComponent.h"
)
source_group("Components\\Music" FILES ${Components__Music})

set(Components__Physics
    "../source/components/SpritePhysics.h"
    "../source/components/SpritePhysicsComponent.h"
)
source_group("Components\\Physics" FILES ${Components__Physics})

set(Components__SFX
    "../source/components/SFXComponent.h"
)
source_group("Components\\SFX" FILES ${Components__SFX})

set(Components__Script
    "../source/components/ScriptComponent.h"
)
source_group("Components\\Script" FILES ${Components__Script})

set(Components__UI
    "../source/components/UIComponent.h"
)
source_group("Components\\UI" FILES ${Components__UI})

set(Foundation
    "../source/Clock.cpp"
    "../source/Clock.h"
    "../source/Config.h"
    "../source/DoubleBufferedList.h"
    "../source/EventArgs.h"
    "../source/Factory.h"
    "../source/IPakalApplication.h"
    "../source/PakalMath.cpp"
    "../source/PakalMath.h"
    "../source/PakalTime.cpp"
    "../source/PakalTime.h"
    "../source/Platform.h"
    "../source/RTTI.h"
    "../source/SimpleTimer.cpp"
    "../source/SimpleTimer.h"
    "../source/State.cpp"
    "../source/State.h"
    "../source/StateMachine.cpp"
    "../source/StateMachine.h"
    "../source/System.cpp"
    "../source/System.h"
    "../source/ThreadPool.cpp"
    "../source/ThreadPool.h"
    "../source/Timer.cpp"
    "../source/Timer.h"
    "../source/Utils.h"
    "../source/win32/ClockImpl.h"
)
source_group("Foundation" FILES ${Foundation})

set(Foundation__Persist
    "../source/persist/Archive.cpp"
    "../source/persist/Archive.h"
    "../source/persist/Attribute.cpp"
    "../source/persist/Attribute.h"
    "../source/persist/Element.cpp"
    "../source/persist/Element.h"
    "../source/persist/JsonReader.cpp"
    "../source/persist/JsonReader.h"
    "../source/persist/JsonWriter.cpp"
    "../source/persist/JsonWriter.h"
    "../source/persist/PersistUtils.h"
    "../source/persist/TextReader.cpp"
    "../source/persist/TextReader.h"
    "../source/persist/TextWriter.cpp"
    "../source/persist/TextWriter.h"
    "../source/persist/XmlReader.cpp"
    "../source/persist/XmlReader.h"
    "../source/persist/XmlWriter.cpp"
    "../source/persist/XmlWriter.h"
)
source_group("Foundation\\Persist" FILES ${Foundation__Persist})

set(Managers
    "../source/IManager.h"
)
source_group("Managers" FILES ${Managers})

set(Managers__ComponentManager
    "../source/Component.h"
    "../source/ComponentFactory.h"
    "../source/ComponentManager.cpp"
    "../source/ComponentManager.h"
    "../source/IComponentProvider.h"
)
source_group("Managers\\ComponentManager" FILES ${Managers__ComponentManager})

set(Managers__EntityManager
    "../source/Entity.cpp"
    "../source/Entity.h"
    "../source/EntityDescriptor.cpp"
    "../source/EntityDescriptor.h"
    "../source/EntityManager.cpp"
    "../source/EntityManager.h"
    "../source/GenericEntity.cpp"
    "../source/GenericEntity.h"
    "../source/IEntityProvider.h"
)
source_group("Managers\\EntityManager" FILES ${Managers__EntityManager})

set(Managers__EventManager
    "../source/AsyncTaskDispatcher.cpp"
    "../source/AsyncTaskDispatcher.h"
    "../source/BasicTask.cpp"
    "../source/BasicTask.h"
    "../source/Event.h"
    "../source/EventScheduler.cpp"
    "../source/EventScheduler.h"
    "../source/EventSystemUtils.cpp"
    "../source/EventSystemUtils.h"
    "../source/InboxQueue.cpp"
    "../source/InboxQueue.h"
    "../source/Task.h"
    "../source/TaskCompletionSource.h"
    "../source/TaskFwd.h"
)
source_group("Managers\\EventManager" FILES ${Managers__EventManager})

set(Managers__GameStateManager
    "../source/BaseGameState.h"
    "../source/GameStateManager.cpp"
    "../source/GameStateManager.h"
    "../source/script/ScriptableGameState.cpp"
    "../source/script/ScriptableGameState.h"
)
source_group("Managers\\GameStateManager" FILES ${Managers__GameStateManager})

set(Managers__InputManager
    "../source/IInputManager.h"
    "../source/InputDevice.h"
)
source_group("Managers\\InputManager" FILES ${Managers__InputManager})

set(Managers__LogManager
    "../source/LogMgr.cpp"
    "../source/LogMgr.h"
)
source_group("Managers\\LogManager" FILES ${Managers__LogManager})

set(Managers__OSManager
    "../source/OSManager.cpp"
    "../source/OSManager.h"
)
source_group("Managers\\OSManager" FILES ${Managers__OSManager})

set(Managers__ResourceManager
    "../source/PakalPath.h"
    "../source/ResourceManager.cpp"
    "../source/ResourceManager.h"
)
source_group("Managers\\ResourceManager" FILES ${Managers__ResourceManager})

set(Managers__ResourceManager__Sources
    "../source/DirectorySource.h"
    "../source/ISource.h"
    "../source/ZipSource.h"
)
source_group("Managers\\ResourceManager\\Sources" FILES ${Managers__ResourceManager__Sources})

set(Managers__ResourceManager__Streams
    "../source/MemoryStream.h"
)
source_group("Managers\\ResourceManager\\Streams" FILES ${Managers__ResourceManager__Streams})

set(Managers__SoundManager
    "../source/SoundManager.h"
)
source_group("Managers\\SoundManager" FILES ${Managers__SoundManager})

set(Managers__UIManager
    "../source/IUIManager.h"
)
source_group("Managers\\UIManager" FILES ${Managers__UIManager})

set(Math
    "../source/math/matrix.h"
    "../source/math/matrix2.h"
    "../source/math/matrix3.h"
    "../source/math/matrix4.h"
    "../source/math/quaternion.h"
    "../source/math/tm.h"
    "../source/math/tmconfig.h"
    "../source/math/tmg.h"
    "../source/math/vector2.h"
    "../source/math/vector3.h"
    "../source/math/vector4.h"
    "../source/math/vectorn.h"
)
source_group("Math" FILES ${Math})

set(Systems
    "../source/ISystem.h"
)
source_group("Systems" FILES ${Systems})

set(Systems__GraphicsSystem
    "../source/DummyGraphicsSystem.cpp"
    "../source/DummyGraphicsSystem.h"
    "../source/GraphicsSystem.cpp"
    "../source/GraphicsSystem.h"
    "../source/IDebugDrawer.h"
)
source_group("Systems\\GraphicsSystem" FILES ${Systems__GraphicsSystem})

set(Systems__PhysicsSystem
    "../source/PhysicsSystem.cpp"
    "../source/PhysicsSystem.h"
)
source_group("Systems\\PhysicsSystem" FILES ${Systems__PhysicsSystem})

set(ALL_FILES
    ${no_group_source_files}
    ${Components}
    ${Components__GFX}
    ${Components__GFX__DynamicMesh}
    ${Components__GFX__DynamicMesh__poly2tri}
    ${Components__GFX__Mesh}
    ${Components__GFX__Sprite}
    ${Components__GFX__Tiles}
    ${Components__Input}
    ${Components__Music}
    ${Components__Physics}
    ${Components__SFX}
    ${Components__Script}
    ${Components__UI}
    ${Foundation}
    ${Foundation__Persist}
    ${Managers}
    ${Managers__ComponentManager}
    ${Managers__EntityManager}
    ${Managers__EventManager}
    ${Managers__GameStateManager}
    ${Managers__InputManager}
    ${Managers__LogManager}
    ${Managers__OSManager}
    ${Managers__ResourceManager}
    ${Managers__ResourceManager__Sources}
    ${Managers__ResourceManager__Streams}
    ${Managers__SoundManager}
    ${Managers__UIManager}
    ${Math}
    ${Systems}
    ${Systems__GraphicsSystem}
    ${Systems__PhysicsSystem}
)

################################################################################
# Target
################################################################################
add_library(${PROJECT_NAME} STATIC ${ALL_FILES})

use_props(${PROJECT_NAME} "${CMAKE_CONFIGURATION_TYPES}" "${DEFAULT_CXX_PROPS}")
set(ROOT_NAMESPACE pakal)

set_target_properties(${PROJECT_NAME} PROPERTIES
    VS_GLOBAL_KEYWORD "Win32Proj"
)
################################################################################
# Output directory
################################################################################
set_target_properties(${PROJECT_NAME} PROPERTIES
    OUTPUT_DIRECTORY_DEBUG_STATIC   "${CMAKE_SOURCE_DIR}//build/$<CONFIG>/"
    OUTPUT_DIRECTORY_DEBUG          "${CMAKE_SOURCE_DIR}//build/$<CONFIG>/"
    OUTPUT_DIRECTORY_RELEASE_STATIC "${CMAKE_SOURCE_DIR}//build/$<CONFIG>/"
    OUTPUT_DIRECTORY_RELEASE        "${CMAKE_SOURCE_DIR}//build/$<CONFIG>/"
)
set_target_properties(${PROJECT_NAME} PROPERTIES
    INTERPROCEDURAL_OPTIMIZATION_RELEASE_STATIC "TRUE"
    INTERPROCEDURAL_OPTIMIZATION_RELEASE        "TRUE"
)
################################################################################
# Include directories
################################################################################
target_include_directories(${PROJECT_NAME} PRIVATE
    "${CMAKE_CURRENT_SOURCE_DIR}/mnt/d/development/PakalCore/source;"
    "C:/Program Files (x86)/Visual Leak Detector/include;"
    "${CMAKE_CURRENT_SOURCE_DIR}/../source"
)

################################################################################
# Compile definitions
################################################################################
target_compile_definitions(${PROJECT_NAME} PRIVATE
    "$<$<CONFIG:Debug_Static>:"
        "_DEBUG;"
        "PAKAL_STATIC_LIB;"
        "_IRR_STATIC_LIB_;"
        "POCO_STATIC;"
        "SFML_STATIC;"
        "OV_EXCLUDE_STATIC_CALLBACKS;"
        "FLAC__NO_DLL;"
        "ROCKET_STATIC_LIB;"
        "_PAKAL_LIB_;"
        "PERSIST_STATIC_LIB"
    ">"
    "$<$<CONFIG:Debug>:"
        "_DEBUG;"
        "OV_EXCLUDE_STATIC_CALLBACKS;"
        "FLAC__NO_DLL;"
        "ROCKET_STATIC_LIB;"
        "PERSIST_NONCLIENT_BUILD;"
        "SFML_STATIC;"
        "_IRR_STATIC_LIB_;"
        "LUA_LIB;"
        "LUA_CORE"
    ">"
    "$<$<CONFIG:release_static>:"
        "NDEBUG;"
        "_IRR_STATIC_LIB_;"
        "POCO_STATIC;"
        "PAKAL_STATIC_LIB;"
        "SFML_STATIC;"
        "OV_EXCLUDE_STATIC_CALLBACKS;"
        "FLAC__NO_DLL;"
        "ROCKET_STATIC_LIB;"
        "PERSIST_STATIC_LIB"
    ">"
    "$<$<CONFIG:Release>:"
        "NDEBUG"
    ">"
    "WIN32;"
    "_WINDOWS;"
    "PAKAL_NONCLIENT_BUILD;"
    "_MBCS"
)

################################################################################
# Compile and link options
################################################################################
if(MSVC)
    target_compile_options(${PROJECT_NAME} PRIVATE
        $<$<CONFIG:Debug_Static>:
            /Gm-;
            /Od;
            /RTC1;
            /MDd;
            /ZI;
            /GR-;
            /MP
        >
        $<$<CONFIG:Debug>:
            /Gm-;
            /Od;
            /RTC1;
            /MDd;
            /ZI
        >
        $<$<CONFIG:release_static>:
            /GL;
            /Gm-;
            /O2;
            /Ob1;
            /Oi;
            /GF;
            /arch:SSE2;
            /MD;
            /Gy;
            /Zi;
            /Ot;
            /GR-;
            /GS-
        >
        $<$<CONFIG:Release>:
            /O2;
            /Ob2;
            /Oi;
            /arch:SSE2;
            /MD;
            /Gy;
            /Zi;
            /Ot;
            /MP
        >
        /W3;
        ${DEFAULT_CXX_EXCEPTION_HANDLING};
        /Y-
    )
    target_link_options(${PROJECT_NAME} PRIVATE
        $<$<CONFIG:Debug>:
            /DEBUG
        >
        $<$<CONFIG:Release>:
            /DEBUG;
            /MACHINE:X86;
            /SUBSYSTEM:WINDOWS;
            /OPT:REF;
            /OPT:ICF;
            /INCREMENTAL:NO
        >
    )
endif()

################################################################################
# Dependencies
################################################################################
set(ADDITIONAL_LIBRARY_DEPENDENCIES
    "$<$<CONFIG:Debug>:"
        "winmm"
    ">"
    "$<$<CONFIG:Release>:"
        "cAudio;"
        "oolua;"
        "lua;"
        "sqlite;"
        "OpenAL32;"
        "tinythread"
    ">"
)
target_link_libraries(${PROJECT_NAME} PUBLIC "${ADDITIONAL_LIBRARY_DEPENDENCIES}")

target_link_directories(${PROJECT_NAME} PUBLIC
    "$<$<CONFIG:Debug>:"
        "${CMAKE_SOURCE_DIR}//build/$<CONFIG>;"
        "${CMAKE_SOURCE_DIR}//build/debug_static;"
        "C:/Program Files (x86)/Visual Leak Detector/lib/Win32;"
        "$ENV{OGRE_HOME}/boost/lib;"
        "$ENV{OGRE_HOME}/lib/$<CONFIG>;"
        "${CMAKE_SOURCE_DIR}//libs/SFML-2.1/lib"
    ">"
    "$<$<CONFIG:Release>:"
        "${CMAKE_SOURCE_DIR}//bin/$<CONFIG>;"
        "${CMAKE_SOURCE_DIR}//libs/_bin/$<CONFIG>;"
        "C:/Program Files (x86)/Visual Leak Detector/lib/Win32;"
        "$ENV{OGRE_HOME}/boost/lib;"
        "$ENV{OGRE_HOME}/lib/$<CONFIG>;"
        "${CMAKE_SOURCE_DIR}//libs/SFML-2.1/lib"
    ">"
)

