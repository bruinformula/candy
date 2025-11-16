include(ExternalProject)

# Option to control whether we build Boost ourselves
option(BUILD_BOOST "Build Boost from source instead of using system version" OFF)

# Allow user to specify a custom Boost root
set(Boost_ROOT "" CACHE PATH "Path to a custom Boost installation")

if(BUILD_BOOST)
    message(STATUS "Building Boost from source...")

    set(BOOST_INSTALL_DIR ${CMAKE_BINARY_DIR}/_deps/boost-install)
    set(BOOST_BINARY_DIR  ${CMAKE_BINARY_DIR}/_deps/boost-build)
    set(BOOST_SOURCE_DIR  ${CMAKE_BINARY_DIR}/_deps/boost-src)

    # Check if Boost is already built
    if(EXISTS ${BOOST_INSTALL_DIR})
        message(STATUS "Boost is already built at ${BOOST_INSTALL_DIR}, skipping build.")
    else()
        include(ExternalProject)
        ExternalProject_Add(
            boost
            GIT_REPOSITORY https://github.com/boostorg/boost.git
            GIT_TAG boost-1.89.0
            GIT_PROGRESS TRUE
            PREFIX ${CMAKE_BINARY_DIR}/_deps/boost
            SOURCE_DIR ${BOOST_SOURCE_DIR}
            BINARY_DIR ${BOOST_BINARY_DIR}
            INSTALL_DIR ${BOOST_INSTALL_DIR}

            CMAKE_ARGS
                -DCMAKE_BUILD_TYPE=${CMAKE_BUILD_TYPE}
                -DCMAKE_INSTALL_PREFIX=${BOOST_INSTALL_DIR}
        )
    endif()

    set(Boost_INCLUDE_DIRS "${BOOST_INSTALL_DIR}/include")
    file(GLOB Boost_LIBS "${BOOST_INSTALL_DIR}/lib/*.a" "${BOOST_INSTALL_DIR}/lib/*.so")

    message(STATUS "Using locally built Boost: ${Boost_INCLUDE_DIRS}")

elseif(Boost_ROOT)
    message(STATUS "Using custom Boost installation at: ${Boost_ROOT}")

    set(Boost_INCLUDE_DIRS "${Boost_ROOT}/include")
    file(GLOB Boost_LIBS "${Boost_ROOT}/lib/*.a" "${Boost_ROOT}/lib/*.so")

    message(STATUS "Found Boost: ${Boost_INCLUDE_DIRS}")
else()
    message(STATUS "Using system-installed Boost")
    find_package(Boost CONFIG REQUIRED)

    set(Boost_LIBS ${Boost_LIBRARIES})
    message(STATUS "Found Boost: ${Boost_INCLUDE_DIRS}")
endif()
