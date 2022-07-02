cmake_minimum_required(VERSION 3.18)
project(
    "chat_service"
    LANGUAGES C CXX
)

set(CMAKE_EXPORT_COMPILE_COMMANDS ON)
set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -Wall -Wextra -pedantic -O2")
set(SRC_DIR "${CMAKE_CURRENT_SOURCE_DIR}/src")
set(SERVER_SOURCES
    "${SRC_DIR}/server.cpp"
    )
set(CLIENT_SOURCES
    "${SRC_DIR}/client.cpp"
    )
set(HEADERS
    "${SRC_DIR}/sockutils.hpp"
)


add_executable(server ${SERVER_SOURCES} ${HEADERS})
add_executable(client ${CLIENT_SOURCES} ${HEADERS})
target_include_directories(server PRIVATE "${SRC_DIR}")
target_include_directories(client PRIVATE "${SRC_DIR}")