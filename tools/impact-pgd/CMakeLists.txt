cmake_minimum_required(VERSION 3.16)
project(impact_pgd_decryptor LANGUAGES C CXX)

if(NOT DEFINED PPSSPP_LIBKIRK_DIR OR NOT EXISTS "\${PPSSPP_LIBKIRK_DIR}/CMakeLists.txt")
    message(FATAL_ERROR "Set PPSSPP_LIBKIRK_DIR to the prepared PPSSPP libkirk source directory")
endif()

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

add_subdirectory("\${PPSSPP_LIBKIRK_DIR}" "\${CMAKE_BINARY_DIR}/ppsspp-libkirk")

add_executable(impact_pgd_decryptor impact_pgd_decryptor.cpp)
target_include_directories(impact_pgd_decryptor PRIVATE "\${PPSSPP_LIBKIRK_DIR}")
target_link_libraries(impact_pgd_decryptor PRIVATE kirk)

enable_testing()
add_test(NAME impact_pgd_self_test COMMAND impact_pgd_decryptor --self-test)
