include("${CMAKE_SOURCE_DIR}/cmake/GetSQLite.cmake")
include("${CMAKE_SOURCE_DIR}/cmake/GetBoost.cmake")

file(GLOB_RECURSE candy_CXX_FILES CONFIGURE_DEPENDS *.cpp)

add_library(candy STATIC ${candy_CXX_FILES})

target_compile_options(candy PRIVATE
    "-fmodules"
    "-fimplicit-modules"
    "-fimplicit-module-maps"
    )

target_include_directories(candy PUBLIC "${CMAKE_SOURCE_DIR}/include")

#add_dependencies(candy boost)
target_link_libraries(candy PUBLIC ${Boost_LIBS})
target_include_directories(candy PUBLIC ${Boost_INCLUDE_DIRS})


if (NOT SQL_USE_SYSTEM)
    target_include_directories(candy PUBLIC ${sqlite3_ext_SOURCE_DIR} )
    target_link_libraries(candy PRIVATE SQLite3)
else()
    target_link_libraries(candy PRIVATE SQLite::SQLite3)
endif()