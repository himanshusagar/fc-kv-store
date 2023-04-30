# CMakeLists.txt
cmake_minimum_required(VERSION 3.10)

project(fc_kv_store)

file(GLOB SRCS_Store server.cc store.h store.cc)
file(GLOB SRCS_Customer customer.cc)


add_executable(simple_kv_store ${SRCS_Store})
add_executable(customer ${SRCS_Customer})


target_link_libraries(simple_kv_store
        Threads::Threads
        gRPC::grpc++
        p3protolib
        leveldb::leveldb)

add_dependencies(simple_kv_store p3protolib)
set_target_properties(simple_kv_store PROPERTIES RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin/)


target_link_libraries(customer
        Threads::Threads
        gRPC::grpc++
        p3protolib
        leveldb::leveldb)

add_dependencies(customer p3protolib)
set_target_properties(customer PROPERTIES RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin/)
