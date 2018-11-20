cmake_minimum_required(VERSION 3.12)
project(A03_pgm)

#set(CMAKE_CXX_STANDARD 11)
set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -std=c++11 -pthread")

add_executable(A03_pgm  manager.cpp)