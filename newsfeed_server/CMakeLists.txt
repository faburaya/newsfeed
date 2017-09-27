###################################################
# CMake build script for newsfeed/newsfeed_server
#

cmake_minimum_required(VERSION 2.6)

project(newsfeed_server)

set(BOOST_HOME "/opt/boost-1.65.1")
set(POCO_ROOT "/opt/poco-1.7.8p3")
set(GRPC_INSTALLATION "/usr/local")

########################
# Include directories:

include_directories(
    "${PROJECT_SOURCE_DIR}/include"
    "${PROJECT_SOURCE_DIR}/../common/include"
    "${POCO_ROOT}/include"
    "${BOOST_HOME}/include"
)

########################
# Dependency libraries:

# How and what libs to link:
add_library(common               STATIC IMPORTED)
add_library(boost_system         STATIC IMPORTED)
add_library(boost_thread         STATIC IMPORTED)
add_library(aws-cpp-sdk-dynamodb STATIC IMPORTED)
add_library(aws-cpp-sdk-core     STATIC IMPORTED)
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
add_library(PocoUtil             STATIC IMPORTED)
add_library(PocoXML              STATIC IMPORTED)
add_library(PocoFoundation       STATIC IMPORTED)

# Where the lib binaries are:
set_target_properties(aws-cpp-sdk-dynamodb PROPERTIES IMPORTED_LOCATION "$ENV{AWS_SDK_INSTALLATION}/libaws-cpp-sdk-dynamodb.a")
set_target_properties(aws-cpp-sdk-core     PROPERTIES IMPORTED_LOCATION "$ENV{AWS_SDK_INSTALLATION}/libaws-cpp-sdk-core.a")
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

string(TOLOWER ${CMAKE_BUILD_TYPE} buildType)
if(buildType STREQUAL release)
    add_definitions(-DNDEBUG)
    set_target_properties(common          PROPERTIES IMPORTED_LOCATION "${CMAKE_CURRENT_BINARY_DIR}/../common/libcommon.a")
    set_target_properties(PocoUtil        PROPERTIES IMPORTED_LOCATION "${POCO_ROOT}/lib/libPocoUtil.a")
    set_target_properties(PocoXML         PROPERTIES IMPORTED_LOCATION "${POCO_ROOT}/lib/libPocoXML.a")
    set_target_properties(PocoFoundation  PROPERTIES IMPORTED_LOCATION "${POCO_ROOT}/lib/libPocoFoundation.a")
    set_target_properties(boost_system PROPERTIES IMPORTED_LOCATION "${BOOST_HOME}/lib/libboost_system-mt.a")
    set_target_properties(boost_thread PROPERTIES IMPORTED_LOCATION "${BOOST_HOME}/lib/libboost_thread-mt.a")
elseif(buildType STREQUAL debug)
    set_target_properties(common          PROPERTIES IMPORTED_LOCATION "${CMAKE_CURRENT_BINARY_DIR}/../common/libcommond.a")
    set_target_properties(PocoUtil        PROPERTIES IMPORTED_LOCATION "${POCO_ROOT}/lib/libPocoUtild.a")
    set_target_properties(PocoXML         PROPERTIES IMPORTED_LOCATION "${POCO_ROOT}/lib/libPocoXMLd.a")
    set_target_properties(PocoFoundation  PROPERTIES IMPORTED_LOCATION "${POCO_ROOT}/lib/libPocoFoundationd.a")
    set_target_properties(boost_system PROPERTIES IMPORTED_LOCATION "${BOOST_HOME}/lib/libboost_system-mt-d.a")
    set_target_properties(boost_thread PROPERTIES IMPORTED_LOCATION "${BOOST_HOME}/lib/libboost_thread-mt-d.a")
endif()

# Place the configuration file along with the executable:
add_custom_command(
   OUTPUT newsfeed_server.config
   COMMAND cp ${PROJECT_SOURCE_DIR}/configuration.xml ${CMAKE_CURRENT_BINARY_DIR}/../bin/newsfeed_server.config
   DEPENDS ${PROJECT_SOURCE_DIR}/configuration.xml
)

# Executable source files:
add_executable(newsfeed_server
    configuration.cpp
    DbConnPool.cpp
    DDBAccess.cpp
    main.cpp
    server_impl.cpp
    newsfeed_server.config
)

target_link_libraries(newsfeed_server 
    common
    aws-cpp-sdk-dynamodb aws-cpp-sdk-core
    grpc++ grpc++_unsecure grpc++_reflection grpc++_cronet grpc++_error_details
    grpc grpc_unsecure grpc_cronet gpr
    protobuf
    curl
    ssl crypto
    pthread
    PocoUtil PocoXML PocoFoundation
    boost_system boost_thread
)

install (TARGETS newsfeed_server DESTINATION "${PROJECT_SOURCE_DIR}/../bin")
