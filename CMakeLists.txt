cmake_minimum_required(VERSION 3.20)
project(gTerminal)

set(CMAKE_CXX_STANDARD 17)

add_library(gTerminal SHARED gTerminal.cpp gTerminal.inl)
target_compile_definitions(gTerminal PRIVATE GTERMINAL_EXPORTS)

#add warnings
if (MSVC)
    target_compile_options(gTerminal PRIVATE /W4 /WX)
else()
    target_compile_options(gTerminal PRIVATE -Wall -Wextra -pedantic)
endif()

#test
add_executable(test test.cpp)
target_link_libraries(test gTerminal)
