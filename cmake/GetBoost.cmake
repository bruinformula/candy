include(FetchContent)


set(BOOST_USE_SYSTEM ON CACHE BOOL "Use system-installed Boost libraries if available")
# Optionally specify a local Boost path
# Usage: cmake -DBOOST_LOCAL_PATH=/path/to/boost ..
if(DEFINED BOOST_USE_SYSTEM AND BOOST_USE_SYSTEM)
    find_package(Boost CONFIG REQUIRED)
     message(STATUS "Using system-installed Boost: ${Boost_INCLUDE_DIRS}")
     # You can link to Boost::boost or Boost::filesystem, etc., as needed
else()

     set(BOOST_ENABLE_CMAKE ON)

     FetchContent_Declare(build_boost
          GIT_REPOSITORY https://github.com/boostorg/boost.git
          GIT_TAG boost-1.82.0
     )

     FetchContent_GetProperties(build_boost)

     if(NOT build_boost_POPULATED)
          FetchContent_Populate(build_boost)
          add_subdirectory(
               ${build_boost_SOURCE_DIR}
               ${build_boost_BINARY_DIR}
               EXCLUDE_FROM_ALL
          )
     endif()
endif()