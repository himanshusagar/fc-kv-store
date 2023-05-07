# CMakeLists.txt
cmake_minimum_required(VERSION 3.10)

if (CMAKE_CXX_COMPILER_ID MATCHES "GNU|Clang")
    add_compile_options(-w)
endif()

project(fc_kv_store)

file(GLOB SRCS_Store server.cc server.h)
# file(GLOB SRCS_Customer customer.cc)

file(GLOB SRCS_Customer customer.cc customer.h)

add_library(customer_lib STATIC ${SRCS_Customer})
add_executable(customer customer_main.cc)

target_link_libraries(customer_lib
        Threads::Threads
        gRPC::grpc++
        p3protolib
        leveldb::leveldb)

add_dependencies(customer_lib p3protolib)


add_executable(simple_kv_store ${SRCS_Store})
# add_executable(customer ${SRCS_Customer})


target_link_libraries(simple_kv_store
        Threads::Threads
        gRPC::grpc++
        p3protolib
        leveldb::leveldb)

add_dependencies(simple_kv_store p3protolib)
set_target_properties(simple_kv_store PROPERTIES RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin/)


target_link_libraries(customer
        customer_lib
        Threads::Threads
        gRPC::grpc++
        p3protolib
        leveldb::leveldb)

add_dependencies(customer p3protolib)
set_target_properties(customer PROPERTIES RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin/)
