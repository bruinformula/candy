
cmake_minimum_required(VERSION 3.26)

project(Candy_Project VERSION 0.1 LANGUAGES CXX C ASM)

set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_C_STANDARD 11)
set(CMAKE_C_STANDARD_REQUIRED ON)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)


set(CANDY_BUILD_CORE_ONLY ON CACHE BOOL "Build for MCU/embedded target" FORCE)

set(MX_Defines_Syms 
	USE_PWR_LDO_SUPPLY 
	USE_HAL_DRIVER 
	STM32H723xx
    $<$<CONFIG:Debug>:DEBUG>
)
set(MX_LINK_LIBS 
    hal
    ${TOOLCHAIN_LINK_LIBRARIES}
)

set(MX_include_Dirs
    "${CMAKE_CURRENT_SOURCE_DIR}/include"
)

# STM32CubeMX generated application sources
file(GLOB_RECURSE MX_Application_src CONFIGURE_DEPENDS 
    "${CMAKE_CURRENT_SOURCE_DIR}/src/*.c"
)


# Interface library for includes and symbols
add_library(stm32cubemx INTERFACE)
target_include_directories(stm32cubemx INTERFACE ${MX_include_Dirs})
target_compile_definitions(stm32cubemx INTERFACE ${MX_Defines_Syms})

target_link_directories(stm32cubemx INTERFACE ${MX_LINK_DIRS})

target_link_libraries(stm32cubemx INTERFACE ${MX_LINK_LIBS})

set_target_properties(stm32cubemx PROPERTIES ADDITIONAL_CLEAN_FILES ${CMAKE_PROJECT_NAME}.map)

# Validate that STM32CubeMX code is compatible with C standard
if((CMAKE_C_STANDARD EQUAL 90) OR (CMAKE_C_STANDARD EQUAL 99))
    message(ERROR "Generated code requires C11 or higher")
endif()

# Setup compiler settings

add_executable(${CMAKE_PROJECT_NAME})

set_target_properties(${CMAKE_PROJECT_NAME} PROPERTIES LINKER_LANGUAGE CXX)

target_link_directories(${CMAKE_PROJECT_NAME} PRIVATE
)

target_sources(${CMAKE_PROJECT_NAME} PRIVATE
    "${CMAKE_CURRENT_SOURCE_DIR}/startup_stm32h723xx.s"
    ${MX_Application_src}
)

target_include_directories(${CMAKE_PROJECT_NAME} PRIVATE
)

target_compile_definitions(${CMAKE_PROJECT_NAME} PRIVATE
    # Add user defined symbols
)

# Remove wrong libob.a library dependency when using cpp files
list(REMOVE_ITEM CMAKE_C_IMPLICIT_LINK_LIBRARIES ob)

# Add linked libraries
target_link_libraries(${CMAKE_PROJECT_NAME}
    stm32cubemx
    hal

    # Add user defined libraries
)