#DBC Parser Test
include("${CMAKE_SOURCE_DIR}/cmake/GetBoostSpirit.cmake")

add_executable(test_v2c V2CTest.cpp)

target_include_directories(test_v2c PRIVATE "${CMAKE_SOURCE_DIR}/include/")

target_link_libraries(test_v2c PRIVATE can_land)

if (NOT BOOST_USE_SYSTEM)
    target_include_directories(test_v2c PRIVATE include ${boost_SOURCE_DIR})
    target_link_libraries(test_v2c PRIVATE boost_spirit)
else()
    target_link_libraries(test_v2c PRIVATE Boost::boost)
endif()

file(COPY "${CMAKE_CURRENT_SOURCE_DIR}/network.dbc" DESTINATION "${CMAKE_CURRENT_BINARY_DIR}")

#SQL Test

add_executable(test_sql SQLTest.cpp)

target_include_directories(test_sql PRIVATE "${CMAKE_SOURCE_DIR}/include/")

target_link_libraries(test_sql PRIVATE can_land)

if (NOT BOOST_USE_SYSTEM)
    target_include_directories(test_sql PRIVATE include ${boost_SOURCE_DIR})
    target_link_libraries(test_sql PRIVATE boost_spirit)
else()
    target_link_libraries(test_sql PRIVATE Boost::boost)
endif()

#CSV Test

add_executable(test_csv CSVTest.cpp)

target_include_directories(test_csv PRIVATE "${CMAKE_SOURCE_DIR}/include/")

target_link_libraries(test_csv PRIVATE can_land)

if (NOT BOOST_USE_SYSTEM)
    target_include_directories(test_csv PRIVATE include ${boost_SOURCE_DIR})
    target_link_libraries(test_csv PRIVATE boost_spirit)
else()
    target_link_libraries(test_csv PRIVATE Boost::boost)
endif()


#Backend Test

add_executable(test_backend BackendTest.cpp)

target_include_directories(test_backend PRIVATE "${CMAKE_SOURCE_DIR}/include/")

target_link_libraries(test_backend PRIVATE can_land)

if (NOT BOOST_USE_SYSTEM)
    target_include_directories(test_backend PRIVATE include ${boost_SOURCE_DIR})
    target_link_libraries(test_backend PRIVATE boost_spirit)
else()
    target_link_libraries(test_backend PRIVATE Boost::boost)
endif()

#Motec Test 

add_executable(test_motec MotecTest.cpp)

target_include_directories(test_motec PRIVATE "${CMAKE_SOURCE_DIR}/include/")

file(COPY "${CMAKE_CURRENT_SOURCE_DIR}/motec_test.csv" DESTINATION "${CMAKE_CURRENT_BINARY_DIR}")

target_link_libraries(test_motec PRIVATE can_land)

if (NOT BOOST_USE_SYSTEM)
    target_include_directories(test_motec PRIVATE include ${boost_SOURCE_DIR})
    target_link_libraries(test_motec PRIVATE boost_spirit)
else()
    target_link_libraries(test_motec PRIVATE Boost::boost)
endif()