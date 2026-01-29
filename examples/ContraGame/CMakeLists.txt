# Contra Game Example
# CMakeLists.txt for building the Contra-style run & gun game example

cmake_minimum_required(VERSION 3.10)

project(ContraGame)

# Set C++ standard
set(CMAKE_CXX_STANDARD 14)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Source files
set(CONTRA_SOURCES
    main.cpp
    ContraGame.cpp
    
    # Components
    Components/Health.cpp
    Components/Weapon.cpp
    Components/PlayerController.cpp
    Components/EnemyAI.cpp
    
    # Systems
    Systems/CollisionSystem.cpp
    Systems/SpawnSystem.cpp
    Systems/ScoreSystem.cpp
    
    # Game States
    GameState/MenuState.cpp
    GameState/GamePlayState.cpp
    GameState/GameOverState.cpp
)

# Header files (for IDE organization)
set(CONTRA_HEADERS
    ContraGame.h
    
    # Components
    Components/Health.h
    Components/Weapon.h
    Components/PlayerController.h
    Components/EnemyAI.h
    
    # Systems
    Systems/CollisionSystem.h
    Systems/SpawnSystem.h
    Systems/ScoreSystem.h
    
    # Game States
    GameState/MenuState.h
    GameState/GamePlayState.h
    GameState/GameOverState.h
)

# Create executable
add_executable(ContraGame ${CONTRA_SOURCES} ${CONTRA_HEADERS})

# Include directories
target_include_directories(ContraGame PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}
    ${CMAKE_CURRENT_SOURCE_DIR}/../..  # Pakal root
    ${CMAKE_CURRENT_SOURCE_DIR}/../../source  # Pakal source
)

# Link against Pakal Engine
# Note: This assumes Pakal is built as a library
# Adjust based on your actual build configuration
target_link_libraries(ContraGame PRIVATE
    # pakal_engine  # Uncomment when Pakal is built as a library
    # Box2D
    # SFML (for audio and input)
    # Irrlicht or bgfx (depending on PAKAL_USE_BGFX flag)
)

# Compiler definitions
if(PAKAL_USE_BGFX)
    target_compile_definitions(ContraGame PRIVATE PAKAL_USE_BGFX=1)
endif()

if(PAKAL_USE_IRRLICHT)
    target_compile_definitions(ContraGame PRIVATE PAKAL_USE_IRRLICHT=1)
endif()

# Copy assets to build directory
# add_custom_command(TARGET ContraGame POST_BUILD
#     COMMAND ${CMAKE_COMMAND} -E copy_directory
#     ${CMAKE_CURRENT_SOURCE_DIR}/Assets
#     $<TARGET_FILE_DIR:ContraGame>/Assets
# )

# Installation
install(TARGETS ContraGame
    RUNTIME DESTINATION bin
)

# Install assets
# install(DIRECTORY Assets
#     DESTINATION bin
# )

message(STATUS "ContraGame example configured")
message(STATUS "  bgfx support: ${PAKAL_USE_BGFX}")
message(STATUS "  Irrlicht support: ${PAKAL_USE_IRRLICHT}")
