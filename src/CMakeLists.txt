# CMakeLists.txt
cmake_minimum_required(VERSION 3.10)

project(fc_kv_store)

file(GLOB SRCS_Store store.cc)

add_executable(simple_kv_store ${SRCS_Store})

target_link_libraries(simple_kv_store leveldb::leveldb)

set_target_properties(simple_kv_store PROPERTIES RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin/)
