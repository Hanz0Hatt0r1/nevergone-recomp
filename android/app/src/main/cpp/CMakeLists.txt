cmake_minimum_required(VERSION 3.22.1)
project(nevergone_recomp LANGUAGES C CXX)

include(cmake/Lua523.cmake)

add_library(nevergone_recomp SHARED
    native_bootstrap.cpp
    lua_bootstrap.cpp
)

target_compile_features(nevergone_recomp PRIVATE cxx_std_17)

target_link_libraries(nevergone_recomp PRIVATE
    lua523
    android
    log
)
