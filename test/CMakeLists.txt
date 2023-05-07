cmake_minimum_required(VERSION 3.10)

find_package(GTest REQUIRED)

add_executable(customer_test customer_test.cc)

target_include_directories(customer_test
        PRIVATE
        ${GTEST_INCLUDE_DIRS}
        ${CMAKE_SOURCE_DIR}/src
)

target_link_libraries(customer_test
        GTest::GTest
        GTest::Main
        Threads::Threads
        gRPC::grpc++
        p3protolib
        leveldb::leveldb
        customer_lib
)

gtest_discover_tests(customer_test)
