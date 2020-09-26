cmake_minimum_required(VERSION 2.8)
find_package(Boost COMPONENTS system thread REQUIRED)
include_directories(${Boost_INCLUDE_DIR})
link_directories(${Boost_LIBRARY_DIR})

add_executable(eager eager.cpp)
target_link_libraries( eager ${Boost_LIBRARIES} )
