cmake_minimum_required(VERSION 3.22.1)
project(forecast_jni CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# The parser lives in the repo's forecast-data folder, so there is one copy of it.
# This file is android/app/src/main/cpp, so the repo root is five levels up.
set(FD_DIR ${CMAKE_CURRENT_SOURCE_DIR}/../../../../../forecast-data)

add_library(forecast_data STATIC ${FD_DIR}/src/forecast_data.cpp)
target_include_directories(forecast_data PUBLIC ${FD_DIR}/include)
target_compile_options(forecast_data PRIVATE -Wall -Wextra -Wpedantic)

add_library(forecast_jni SHARED forecast_jni.cpp)
target_link_libraries(forecast_jni forecast_data)
target_compile_options(forecast_jni PRIVATE -Wall -Wextra)
