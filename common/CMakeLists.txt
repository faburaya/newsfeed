##########################################
# CMake build script for newsfeed/common
#

cmake_minimum_required(VERSION 2.6)

project(common)

# NDEBUG when release mode:
string(TOLOWER ${CMAKE_BUILD_TYPE} buildType)
if(buildType STREQUAL release)
    add_definitions(-DNDEBUG)
endif()

set(POCO_ROOT "/opt/poco-1.7.8p3")

# Include directories:
include_directories(
    "${PROJECT_SOURCE_DIR}/include"
    "${POCO_ROOT}/include"
)

# Static library source files:
add_library(common STATIC
    common.cpp
    newsfeed_messages.pb.cc
    newsfeed_service.grpc.pb.cc
)

