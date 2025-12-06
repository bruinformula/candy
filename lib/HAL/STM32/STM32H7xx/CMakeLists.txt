
set(STM32 "STM32H7xx")
file(GLOB hal_C_FILES CONFIGURE_DEPENDS "${CMAKE_CURRENT_SOURCE_DIR}/*.c")

set(STM32_INCLUDE_DIRS
    "${CMAKE_SOURCE_DIR}/include/HAL/STM32/CMSIS" 
    "${CMAKE_SOURCE_DIR}/include/HAL/STM32/${STM32}" 
    "${CMAKE_SOURCE_DIR}/include/HAL/STM32/${STM32}/Driver"
    "${CMAKE_SOURCE_DIR}/include/HAL/STM32/${STM32}/Driver/Legacy"
)

add_library(hal OBJECT)
target_sources(hal PRIVATE ${hal_C_FILES})
target_link_libraries(hal PUBLIC stm32cubemx)

target_include_directories(hal PUBLIC ${STM32_INCLUDE_DIRS})
target_compile_options(hal PUBLIC
    "-flto" "-fno-exceptions" "-fno-rtti" "-ffunction-sections" "-fdata-sections"
)