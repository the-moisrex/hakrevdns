cmake_minimum_required(VERSION 3.10)
project(hakrevdns)
set(CMAKE_CXX_STANDARD 11)
add_executable(hakrevdns main.cpp)
target_link_libraries(hakrevdns pthread)
