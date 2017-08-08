cmake_minimum_required (VERSION 2.8.10.2)
project(hex2bix)

if(NOT DEFINED CMAKE_BUILD_TYPE)
set(CMAKE_BUILD_TYPE )
endif(NOT DEFINED CMAKE_BUILD_TYPE)

if(APPLE)
set(CMAKE_CXX_FLAGS_RELEASE "-O3 -std=c++14")
set(CMAKE_CXX_FLAGS_DEBUG   "-O0 -g -std=c++14")
set(CMAKE_C_FLAGS_RELEASE   "-O3 -std=c11")
set(CMAKE_C_FLAGS_DEBUG     "-O0 -g -std=c11")
else(APPLE)
set(CMAKE_CXX_FLAGS_RELEASE "-O3 -std=c++11")
set(CMAKE_CXX_FLAGS_DEBUG   "-O0 -ggdb -g3 -std=c++11")
set(CMAKE_C_FLAGS_RELEASE   "-O3 -std=c11")
set(CMAKE_C_FLAGS_DEBUG     "-O0 -ggdb -g3 -std=c11")
endif(APPLE)

set(HEX2BIX_SOURCE_FILES  source/hex2bix.cpp)

include_directories(${CMAKE_INCLUDE_PATH})
add_executable(hex2bix ${HEX2BIX_SOURCE_FILES} ${HEX2BIX_HEADERS} )
if(APPLE)
target_link_libraries(hex2bix ${INSTALL_DIR}/lib/libdocopt.a stdc++ "-framework Foundation" "-framework IOKit" )
else(APPLE)
target_link_libraries(hex2bix ${INSTALL_DIR}/lib/libdocopt.a pthread)
endif(APPLE)

install(TARGETS hex2bix DESTINATION bin)

