#V2C Parser Test
add_executable(test_v2c V2CTest.cpp)

target_include_directories(test_v2c PRIVATE "${CMAKE_SOURCE_DIR}/include/")

target_link_libraries(test_v2c PRIVATE candy)

file(COPY "${CMAKE_CURRENT_SOURCE_DIR}/network.dbc" DESTINATION "${CMAKE_CURRENT_BINARY_DIR}")

#SQL Test

add_executable(test_sql SQLTest.cpp)

target_include_directories(test_sql PRIVATE "${CMAKE_SOURCE_DIR}/include/")

target_link_libraries(test_sql PRIVATE candy)

#CSV Test

add_executable(test_csv CSVTest.cpp)

target_include_directories(test_csv PRIVATE "${CMAKE_SOURCE_DIR}/include/")

target_link_libraries(test_csv PRIVATE candy)

#Backend Test

add_executable(test_backend BackendTest.cpp)

target_include_directories(test_backend PRIVATE "${CMAKE_SOURCE_DIR}/include/")

target_link_libraries(test_backend PRIVATE candy)

#Motec Test 

add_executable(test_motec MotecTest.cpp)

target_include_directories(test_motec PRIVATE "${CMAKE_SOURCE_DIR}/include/")

file(COPY "${CMAKE_CURRENT_SOURCE_DIR}/motec_test.csv" DESTINATION "${CMAKE_CURRENT_BINARY_DIR}")

target_link_libraries(test_motec PRIVATE candy)