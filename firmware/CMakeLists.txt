# ESP-IDF CMakeLists.txt for Lam Research Challenge 3.0 Stage 2 Controller
cmake_minimum_required(VERSION 3.16)

# Bypass arduino-esp32 version lock for ESP-IDF v5.2.7
set(ENV{ARDUINO_SKIP_IDF_VERSION_CHECK} 1)

include($ENV{IDF_PATH}/tools/cmake/project.cmake)
project(unverified_controller)
