cmake_minimum_required(VERSION 3.28)

project(moon LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

find_package(Threads REQUIRED)
find_package(Boost REQUIRED CONFIG COMPONENTS headers)
find_package(OpenSSL REQUIRED)

add_executable(moon
  src/moon.cpp)

target_link_libraries(moon
  PRIVATE Boost::headers OpenSSL::SSL OpenSSL::Crypto Threads::Threads)
