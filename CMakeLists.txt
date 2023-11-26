cmake_minimum_required(VERSION 3.5)

project(network-module LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

find_package(Poco REQUIRED COMPONENTS Foundation Net)

set(NETWORKING_DIR networking)
set(SOURCES ${NETWORKING_DIR}/networking.cpp)
set(HEADERS ${NETWORKING_DIR}/networking.hpp
            ${NETWORKING_DIR}/net_exceptions.hpp)

add_library(network-module STATIC ${SOURCES} ${HEADERS})
target_include_directories(network-module PUBLIC Poco::Net
                                                 Poco::Foundation)
target_link_libraries(network-module PRIVATE -static-libgcc -static-libstdc++)
target_link_libraries(network-module PRIVATE Poco::Net
                                             Poco::Foundation)

add_executable(test main.cpp)
target_link_libraries(test PRIVATE network-module)
target_link_libraries(test PRIVATE Poco::Foundation)
target_include_directories(test PRIVATE ${NETWORKING_DIR})

