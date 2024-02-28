cmake_minimum_required(VERSION 3.8)

project(main)

add_subdirectory(extern/curlpp)
add_subdirectory(extern/simple-lua)

set(CMAKE_CXX_STANDARD 23)
add_executable(main main.cpp)

target_include_directories(main PRIVATE 
    ${CMAKE_SOURCE_DIR}/include
    ${CMAKE_SOURCE_DIR}/extern/json/include
    ${CMAKE_SOURCE_DIR}/extern/curlpp/include
    ${CMAKE_SOURCE_DIR}/extern/simple-lua/include)

target_compile_definitions(main PRIVATE 
    SOURCE_DIR="${CMAKE_SOURCE_DIR}"
    DATABASE_DIR="${CMAKE_SOURCE_DIR}/files")

target_link_libraries(main PRIVATE curlpp simple-lua)

#add_executable(fdump dump.cpp)
#target_include_directories(fdump PRIVATE ${CMAKE_SOURCE_DIR}/extern/json/include)