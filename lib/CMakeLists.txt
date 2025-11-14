include("${CMAKE_SOURCE_DIR}/cmake/GetBoost.cmake")
include("${CMAKE_SOURCE_DIR}/cmake/GetSQLite.cmake")

file(GLOB_RECURSE can_land_CXX_FILES CONFIGURE_DEPENDS *.cpp)

add_library(can_land STATIC ${can_land_CXX_FILES})

target_include_directories(can_land PUBLIC "${CMAKE_SOURCE_DIR}/include" "${ggml_SOURCE_DIR}/include")


#target_link_libraries(can_land PRIVATE ggml)

if (NOT BOOST_USE_SYSTEM)
    target_include_directories(can_land PRIVATE include ${boost_SOURCE_DIR})
    target_link_libraries(can_land PRIVATE boost_spirit)
else()
    target_link_libraries(can_land PRIVATE Boost::boost)
endif()

if (NOT SQL_USE_SYSTEM)
    target_include_directories(can_land PUBLIC ${sqlite3_ext_SOURCE_DIR} )
    target_link_libraries(can_land PRIVATE SQLite3)
else()
    target_link_libraries(can_land PRIVATE SQLite::SQLite3)
endif()