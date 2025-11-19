include(ExternalProject)

# Option to control whether we build SQLite3 ourselves
option(BUILD_SQLITE3 "Build SQLite3 from source instead of using system version" OFF)

# Allow user to specify a custom SQLite3 root
set(SQLite3_ROOT "" CACHE PATH "Path to a custom SQLite3 installation")

if(BUILD_SQLITE3)
    message(STATUS "Building SQLite3 from source...")

    set(SQLITE3_INSTALL_DIR ${CMAKE_BINARY_DIR}/_deps/sqlite3-install)
    set(SQLITE3_BINARY_DIR  ${CMAKE_BINARY_DIR}/_deps/sqlite3-build)
    set(SQLITE3_SOURCE_DIR  ${CMAKE_BINARY_DIR}/_deps/sqlite3-src)

    if(EXISTS ${SQLITE3_INSTALL_DIR})
        message(STATUS "SQLite3 is already built at ${SQLITE3_INSTALL_DIR}, skipping build.")
    else()
        include(ExternalProject)

        file(MAKE_DIRECTORY ${SQLITE3_BINARY_DIR})

        set(_sqlite_cmakelists "
        cmake_minimum_required(VERSION 3.0)
        project(sqlite3 C)
        add_library(sqlite3 STATIC \"${SQLITE3_SOURCE_DIR}/sqlite3.c\")
        set_target_properties(sqlite3 PROPERTIES OUTPUT_NAME sqlite3 ARCHIVE_OUTPUT_DIRECTORY \"${SQLITE3_INSTALL_DIR}/lib\" LIBRARY_OUTPUT_DIRECTORY \"${SQLITE3_INSTALL_DIR}/lib\")
        install(TARGETS sqlite3 ARCHIVE DESTINATION lib LIBRARY DESTINATION lib RUNTIME DESTINATION bin)
        install(FILES \"${SQLITE3_SOURCE_DIR}/sqlite3.h\" DESTINATION include)
        ")

        file(WRITE ${SQLITE3_BINARY_DIR}/CMakeLists.txt "${_sqlite_cmakelists}")

        ExternalProject_Add(
            sqlite3
            URL "https://sqlite.org/2025/sqlite-amalgamation-3500200.zip"

            PREFIX ${CMAKE_BINARY_DIR}/_deps/sqlite3
            SOURCE_DIR ${SQLITE3_SOURCE_DIR}
            BINARY_DIR ${SQLITE3_BINARY_DIR}
            INSTALL_DIR ${SQLITE3_INSTALL_DIR}

            CONFIGURE_COMMAND ${CMAKE_COMMAND} -S ${SQLITE3_BINARY_DIR} -B ${SQLITE3_BINARY_DIR} -DCMAKE_BUILD_TYPE=${CMAKE_BUILD_TYPE} -DCMAKE_INSTALL_PREFIX=${SQLITE3_INSTALL_DIR}
            BUILD_COMMAND ${CMAKE_COMMAND} --build ${SQLITE3_BINARY_DIR} --parallel
            INSTALL_COMMAND ${CMAKE_COMMAND} --install ${SQLITE3_BINARY_DIR} --prefix ${SQLITE3_INSTALL_DIR}
        )
    endif()

    set(SQLite3_INCLUDE_DIRS "${SQLITE3_INSTALL_DIR}/include")
    file(GLOB SQLite3_LIBS "${SQLITE3_INSTALL_DIR}/lib/*.a")

    message(STATUS "Using locally built SQLite3: ${SQLite3_INCLUDE_DIRS}")

elseif(SQLite3_ROOT)
    message(STATUS "Using custom SQLite3 installation at: ${SQLite3_ROOT}")

    set(SQLite3_INCLUDE_DIRS "${SQLite3_ROOT}/include")
    file(GLOB SQLite3_LIBS "${SQLite3_ROOT}/lib/*.a" "${SQLite3_ROOT}/lib/*.so")

    message(STATUS "Found SQLite3: ${SQLite3_INCLUDE_DIRS}")
else()
    message(STATUS "Using system-installed SQLite3")
    find_package(SQLite3 CONFIG REQUIRED)

    set(SQLite3_LIBS ${SQLite3_LIBRARIES})
    message(STATUS "Found SQLite3: ${SQLite3_INCLUDE_DIRS}")
endif()