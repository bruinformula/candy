add_library(candy STATIC)

option(CANDY_BUILD_CORE_ONLY "Build for MCU/embedded target" OFF)

file(GLOB_RECURSE CANDY_CORE_CXX_FILES CONFIGURE_DEPENDS
    "${CMAKE_CURRENT_SOURCE_DIR}/Core/*.cpp")
target_sources(candy PRIVATE ${CANDY_CORE_CXX_FILES})

if(NOT CANDY_BUILD_CORE_ONLY)
    file(GLOB_RECURSE CANDY_DBC_CXX_FILES CONFIGURE_DEPENDS
        "${CMAKE_CURRENT_SOURCE_DIR}/DBCInterpreters/*.cpp")
    target_sources(candy PRIVATE ${CANDY_DBC_CXX_FILES})

    include("${CMAKE_SOURCE_DIR}/utils/GetSQLite.cmake")
    target_link_libraries(candy PUBLIC ${SQLite3_LIBS})
    target_include_directories(candy PUBLIC ${SQLite3_INCLUDE_DIRS})
else()
    target_compile_definitions(candy PUBLIC CANDY_BUILD_CORE_ONLY)
endif()

if (NOT CANDY_REGULAR_BUILD_ONLY)
    file(GLOB_RECURSE CANDY_SWIFT_CXX_FILES CONFIGURE_DEPENDS
        "${CMAKE_CURRENT_SOURCE_DIR}/Swift/*.cpp")
    target_sources(candy PRIVATE ${CANDY_SWIFT_CXX_FILES})
else()
    target_compile_definitions(candy PUBLIC CANDY_REGULAR_BUILD_ONLY)
endif()


target_include_directories(candy PUBLIC "${CANDY_PROJECT_ROOT}/include")

target_compile_options(candy PUBLIC
    "-flto" "-fno-exceptions" "-fno-rtti" "-ffunction-sections" "-fdata-sections"
)

