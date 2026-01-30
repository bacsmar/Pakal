# ContraGame CMakeLists.txt
project(ContraGame)

# Collect all source files
file(GLOB_RECURSE CONTRA_SOURCES
    "${CMAKE_CURRENT_SOURCE_DIR}/*.cpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/*.h"
)

# Create executable
add_executable(ContraGame ${CONTRA_SOURCES})

# Link with Pakal engine
target_link_libraries(ContraGame
    Pakal
)

# Include directories
target_include_directories(ContraGame PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}
    ${CMAKE_SOURCE_DIR}/source
    ${CMAKE_SOURCE_DIR}/source/bgfx
    ${CMAKE_SOURCE_DIR}/source/box2D
    ${CMAKE_SOURCE_DIR}/external/bgfx/include
    ${CMAKE_SOURCE_DIR}/external/bx/include
    ${CMAKE_SOURCE_DIR}/external/bimg/include
)

# Set C++ standard
set_property(TARGET ContraGame PROPERTY CXX_STANDARD 11)

# Copy assets to build directory
if(EXISTS ${CMAKE_CURRENT_SOURCE_DIR}/Assets)
    add_custom_command(TARGET ContraGame POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy_directory
        ${CMAKE_CURRENT_SOURCE_DIR}/Assets
        $<TARGET_FILE_DIR:ContraGame>/Assets
    )
endif()

# Installation
install(TARGETS ContraGame DESTINATION bin)
