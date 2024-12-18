cmake_minimum_required(VERSION 3.10)
project(easy_rpc)
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(ROOTDIR ${CMAKE_CURRENT_SOURCE_DIR})
include_directories(${ROOTDIR}/include)
set(EXECUTABLE_OUTPUT_PATH ${ROOTDIR}/bin)
add_subdirectory(${ROOTDIR}/src)
add_subdirectory(${ROOTDIR}/rpc-server-test)
# add_subdirectory(${ROOTDIR}/rpc-client-test)


