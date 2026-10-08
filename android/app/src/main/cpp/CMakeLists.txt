cmake_minimum_required(VERSION 3.22.1)
project(nevergone_recomp LANGUAGES CXX)

add_library(nevergone_recomp SHARED
    native_bootstrap.cpp
)

target_compile_features(nevergone_recomp PRIVATE cxx_std_17)

target_link_libraries(nevergone_recomp PRIVATE
    android
    log
)
