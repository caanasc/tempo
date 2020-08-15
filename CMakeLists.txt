cmake_minimum_required(VERSION 3.15)
project(tempo C)

set(CMAKE_C_STANDARD 11)
set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -std=c++11 -lcurl -cjson")
set(CMAKE_EXPORT_COMPILE_COMMANDS on)


add_executable(tempo tempo.c config.h)
target_link_libraries(tempo curl)
find_library(CJSON cjson PATH_SUFFIXES cjson)
target_link_libraries(tempo ${CJSON})

