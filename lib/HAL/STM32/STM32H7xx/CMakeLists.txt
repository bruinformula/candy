cmake_minimum_required(VERSION 3.22)
# Enable CMake support for ASM and C languages
enable_language(C ASM)
# STM32CubeMX generated symbols (macros)
set(MX_Defines_Syms 
	USE_PWR_LDO_SUPPLY 
	USE_HAL_DRIVER 
	STM32H723xx
    $<$<CONFIG:Debug>:DEBUG>
)

set(STM32 "STM32H7xx")
file(GLOB_RECURSE hal_CXX_FILES CONFIGURE_DEPENDS "*.c")

set(STM32_INCLUDE_DIRS
    "${CMAKE_SOURCE_DIR}/include/HAL/STM32/CMSIS" 
    "${CMAKE_SOURCE_DIR}/include/HAL/STM32/${STM32}" 
    "${CMAKE_SOURCE_DIR}/include/HAL/STM32/${STM32}/Driver"
)

add_library(hal STATIC ${hal_CXX_FILES})

target_include_directories(hal PUBLIC ${STM32_INCLUDE_DIRS})
target_link_libraries(hal PUBLIC ${TOOLCHAIN_LINK_LIBRARIES})

target_compile_options(hal PUBLIC
    "-flto" "-fno-exceptions" "-fno-rtti" "-ffunction-sections" "-fdata-sections"
)

#set_target_properties(${CMAKE_PROJECT_NAME} PROPERTIES ADDITIONAL_CLEAN_FILES ${CMAKE_PROJECT_NAME}.map)

# Validate that STM32CubeMX code is compatible with C standard
if((CMAKE_C_STANDARD EQUAL 90) OR (CMAKE_C_STANDARD EQUAL 99))
    message(ERROR "Generated code requires C11 or higher")
endif()
