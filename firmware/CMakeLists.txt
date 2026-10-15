# ESP-IDF CMakeLists.txt for Lam Research Challenge 3.0 Stage 2 Controller
cmake_minimum_required(VERSION 3.16)

# arduino-esp32 3.0.0 officially targets ESP-IDF 5.1.x (max_supported_idf_version 5.1.99).
# Setting ARDUINO_SKIP_IDF_VERSION_CHECK=1 allows building with ESP-IDF 5.2.x.
set(ENV{ARDUINO_SKIP_IDF_VERSION_CHECK} 1)

include($ENV{IDF_PATH}/tools/cmake/project.cmake)
project(team_distro_controller)

# Suppress compiler warnings-as-errors for arduino-esp32 on ESP-IDF v5.2.x
if(TARGET __idf_espressif__arduino-esp32)
    target_compile_options(__idf_espressif__arduino-esp32 PRIVATE
        $<$<COMPILE_LANGUAGE:C>:-Wno-error=implicit-function-declaration -Wno-implicit-function-declaration>
        $<$<COMPILE_LANGUAGE:CXX>:-Wno-error=overloaded-virtual -Wno-overloaded-virtual>
    )
endif()
