###################################################
# CMake build script for newsfeed/newsfeed_client
#

cmake_minimum_required(VERSION 2.6)

project(newsfeed_client)

set(BOOST_HOME "/opt/boost-1.65.1")
set(GRPC_INSTALLATION "/usr/local")

########################
# Include directories:

include_directories(
    "${PROJECT_SOURCE_DIR}/include"
    "${PROJECT_SOURCE_DIR}/../common/include"
    "${BOOST_HOME}/include"
)

########################
# Dependency libraries:

# How and what libs to link:
add_library(common               STATIC IMPORTED)
add_library(boost_system         STATIC IMPORTED)
add_library(boost_thread         STATIC IMPORTED)
add_library(grpc++               STATIC IMPORTED)
add_library(grpc++_unsecure      STATIC IMPORTED)
add_library(grpc++_reflection    STATIC IMPORTED)
add_library(grpc++_cronet        STATIC IMPORTED)
add_library(grpc++_error_details STATIC IMPORTED)
add_library(grpc                 STATIC IMPORTED)
add_library(grpc_unsecure        STATIC IMPORTED)
add_library(grpc_cronet          STATIC IMPORTED)
add_library(gpr                  STATIC IMPORTED)
add_library(protobuf             STATIC IMPORTED)
add_library(zlib                 STATIC IMPORTED)

# Where the lib binaries are:
set_target_properties(grpc++               PROPERTIES IMPORTED_LOCATION "${GRPC_INSTALLATION}/lib/libgrpc++.a")
set_target_properties(grpc++_unsecure      PROPERTIES IMPORTED_LOCATION "${GRPC_INSTALLATION}/lib/libgrpc++_unsecure.a")
set_target_properties(grpc++_reflection    PROPERTIES IMPORTED_LOCATION "${GRPC_INSTALLATION}/lib/libgrpc++_reflection.a")
set_target_properties(grpc++_cronet        PROPERTIES IMPORTED_LOCATION "${GRPC_INSTALLATION}/lib/libgrpc++_cronet.a")
set_target_properties(grpc++_error_details PROPERTIES IMPORTED_LOCATION "${GRPC_INSTALLATION}/lib/libgrpc++_error_details.a")
set_target_properties(grpc                 PROPERTIES IMPORTED_LOCATION "${GRPC_INSTALLATION}/lib/libgrpc.a")
set_target_properties(grpc_unsecure        PROPERTIES IMPORTED_LOCATION "${GRPC_INSTALLATION}/lib/libgrpc_unsecure.a")
set_target_properties(grpc_cronet          PROPERTIES IMPORTED_LOCATION "${GRPC_INSTALLATION}/lib/libgrpc_cronet.a")
set_target_properties(gpr                  PROPERTIES IMPORTED_LOCATION "${GRPC_INSTALLATION}/lib/libgpr.a")
set_target_properties(protobuf             PROPERTIES IMPORTED_LOCATION "${GRPC_INSTALLATION}/lib/libprotobuf.a")
set_target_properties(zlib                 PROPERTIES IMPORTED_LOCATION "${GRPC_INSTALLATION}/lib/libz.a")

string(TOLOWER ${CMAKE_BUILD_TYPE} buildType)
if(buildType STREQUAL release)
    add_definitions(-DNDEBUG)
    set_target_properties(common       PROPERTIES IMPORTED_LOCATION "${CMAKE_CURRENT_BINARY_DIR}/../common/libcommon.a")
    set_target_properties(boost_system PROPERTIES IMPORTED_LOCATION "${BOOST_HOME}/lib/libboost_system-mt.a")
    set_target_properties(boost_thread PROPERTIES IMPORTED_LOCATION "${BOOST_HOME}/lib/libboost_thread-mt.a")
elseif(buildType STREQUAL debug)
    set_target_properties(common       PROPERTIES IMPORTED_LOCATION "${CMAKE_CURRENT_BINARY_DIR}/../common/libcommond.a")
    set_target_properties(boost_system PROPERTIES IMPORTED_LOCATION "${BOOST_HOME}/lib/libboost_system-mt-d.a")
    set_target_properties(boost_thread PROPERTIES IMPORTED_LOCATION "${BOOST_HOME}/lib/libboost_thread-mt-d.a")
endif()

# Executable source files:
add_executable(newsfeed_client
    client.cpp
    InterleavedConsole.cpp
    main.cpp
)

target_link_libraries(newsfeed_client 
    common
    grpc++ grpc++_unsecure grpc++_reflection grpc++_cronet grpc++_error_details
    grpc grpc_unsecure grpc_cronet gpr
    protobuf
    zlib
    ssl crypto
    pthread
    boost_system boost_thread
)

install (TARGETS newsfeed_client DESTINATION "${PROJECT_SOURCE_DIR}/../bin")
