# Set project root for includes
set(CANDY_PROJECT_ROOT "${CMAKE_CURRENT_SOURCE_DIR}/.." CACHE PATH "Candy project root directory")

add_subdirectory("${CMAKE_CURRENT_SOURCE_DIR}/Candy")

if (BUILD_HAL)
    message(STATUS "Building HAL submodule")
    add_subdirectory("${CMAKE_CURRENT_SOURCE_DIR}/HAL")
endif()