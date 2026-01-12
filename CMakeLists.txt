cmake_minimum_required(VERSION 3.15)
project(HighPerfSMTPRelay VERSION 1.0.0 LANGUAGES C)

set(CMAKE_C_STANDARD 11)
set(CMAKE_C_STANDARD_REQUIRED ON)
set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -Wall -Wextra -pthread -D_GNU_SOURCE")

# Release build optimizations
set(CMAKE_C_FLAGS_RELEASE "-O3 -march=native -DNDEBUG")
set(CMAKE_C_FLAGS_DEBUG "-g -O0")

# Dependencies
find_package(OpenSSL REQUIRED)
find_package(Threads REQUIRED)
# Note: libyaml and sqlite3 might not have standard cmake config files on all systems, 
# might need find_library if find_package fails.
find_library(YAML_LIB yaml)
find_library(SQLITE3_LIB sqlite3)
find_path(YAML_INCLUDE_DIR yaml.h)
find_path(SQLITE3_INCLUDE_DIR sqlite3.h)

# Include Directories
include_directories(
    ${CMAKE_SOURCE_DIR}/include 
    ${OPENSSL_INCLUDE_DIR}
    ${YAML_INCLUDE_DIR}
    ${SQLITE3_INCLUDE_DIR}
)

# Source Files
set(SOURCES
    src/main.c
    src/utils/logger.c
    src/utils/mempool.c
    src/server/reactor.c
    src/server/socket_utils.c
    src/server/connection.c
    src/server/smtp.c
    src/utils/config.c
    src/server/storage.c
    src/server/relay.c
    src/server/policy.c
    src/utils/tls.c
    src/utils/queue.c
    src/utils/stats.c
    src/utils/config_reload.c
)

# Executable
add_executable(relaymail ${SOURCES})

# Stress Test Tool
add_executable(stress_test tests/stress.c)
target_link_libraries(stress_test pthread)

# Link Libraries
if(NOT YAML_LIB)
    message(STATUS "libyaml not found via find_library, assuming -lyaml works")
    set(YAML_LIB yaml)
endif()

if(NOT SQLITE3_LIB)
    message(STATUS "libsqlite3 not found via find_library, assuming -lsqlite3 works")
    set(SQLITE3_LIB sqlite3)
endif()

target_link_libraries(relaymail 
    PRIVATE 
    OpenSSL::SSL 
    OpenSSL::Crypto 
    Threads::Threads 
    ${YAML_LIB}
    ${SQLITE3_LIB}
    dl
)

message(STATUS "Build configuration completed for HighPerfSMTPRelay")
