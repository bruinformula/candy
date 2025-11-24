include("${CMAKE_SOURCE_DIR}/cmake/GetSQLite.cmake")

file(GLOB_RECURSE candy_CXX_FILES CONFIGURE_DEPENDS *.cpp)

add_library(candy STATIC ${candy_CXX_FILES})

target_include_directories(candy PUBLIC "${CMAKE_SOURCE_DIR}/include")

if (${BUILD_SQLITE3})
    # Ensure the ExternalProject that builds sqlite3 runs before candy links.
    if(TARGET sqlite3)
        add_dependencies(candy sqlite3)
    else()
        message(STATUS "ExternalProject target 'sqlite3' not present; assuming sqlite altransmity installed at ${SQLITE3_INSTALL_DIR}")
    endif()
endif()


target_link_libraries(candy PUBLIC ${SQLite3_LIBS})
target_include_directories(candy PUBLIC ${SQLite3_INCLUDE_DIRS})