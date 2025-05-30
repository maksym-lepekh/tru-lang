cmake_minimum_required(VERSION 3.31)
project(tru VERSION 0.1 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 23)

include(cmake/CPM.cmake)
CPMAddPackage("gh:Neargye/magic_enum#v0.9.7")

add_executable(truc)
target_sources(truc PRIVATE main.cpp)
target_link_libraries(truc PRIVATE magic_enum::magic_enum)
