cmake_minimum_required(VERSION 2.8)

project(thread-pool-cpp CXX)
set(CMAKE_CXX_STANDARD 20)

set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -std=c++11 -Wall -Wextra")
if(COVERAGE)
    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -g --coverage")
    set(CMAKE_EXE_LINKER_FLAGS "${CMAKE_EXE_LINKER_FLAGS} --coverage")
endif()

# gtest
enable_testing()
add_subdirectory(googletest)

add_compile_options(-fsanitize=thread)
add_link_options(-fsanitize=thread)


include_directories(${CMAKE_CURRENT_SOURCE_DIR}/googletest/googletest/include)
include_directories(${CMAKE_CURRENT_SOURCE_DIR}/include)

# Tests
add_subdirectory(tests)

# Benchmark
add_subdirectory(benchmark)

# Install
file(GLOB_RECURSE INSTALL_FILES_LIST "${CMAKE_CURRENT_SOURCE_DIR}/include/*")
set_source_files_properties(${INSTALL_FILES_LIST} PROPERTIES HEADER_FILE_ONLY 1)
add_library(HEADER_ONLY_TARGET STATIC ${INSTALL_FILES_LIST})
set_target_properties(HEADER_ONLY_TARGET PROPERTIES LINKER_LANGUAGE CXX)

target_compile_options(HEADER_ONLY_TARGET PRIVATE -fsanitize=thread)
target_link_options(HEADER_ONLY_TARGET PRIVATE -fsanitize=thread)


install(DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}/include/" DESTINATION "include")
