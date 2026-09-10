cmake_minimum_required(VERSION 3.10)
project(5G_Link_Adaptation)

# Require C++17 standard
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED True)

# Enable aggressive warnings for clean code
set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -Wall -Wextra -Wpedantic")

add_executable(link_adaptation main.cpp)
