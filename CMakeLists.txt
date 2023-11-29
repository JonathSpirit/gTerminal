cmake_minimum_required(VERSION 3.27)
project(gTerminal)

set(CMAKE_CXX_STANDARD 17)

add_library(gTerminal SHARED gTerminal.cpp gTerminal.inl)
target_compile_definitions(gTerminal PRIVATE GTERMINAL_EXPORTS)

#test
add_executable(test test.cpp)
target_link_libraries(test gTerminal)
