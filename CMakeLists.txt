# The following lines of boilerplate have to be in your project's CMakeLists
# in this exact order for cmake to work correctly
cmake_minimum_required(VERSION 3.20)

include($ENV{IDF_PATH}/tools/cmake/project.cmake)

# add the component directories that we want to use
set(EXTRA_COMPONENT_DIRS
  "components/"
)

# set the hal component to use based on the target
if(IDF_TARGET STREQUAL "esp32")
  #append the components that we want to use
  set(HAL_COMPONENTS "wrover-kit")
  message(WARNING "ESP32 target, using components: ${HAL_COMPONENTS}")
elseif(IDF_TARGET STREQUAL "esp32s3")
  set(HAL_COMPONENTS "t-deck esp-box")
  message(WARNING "ESP32-S3 target, using components: ${HAL_COMPONENTS}")
else()
  message(FATAL_ERROR "Unsupported target: ${IDF_TARGET}")
endif()

set(
  COMPONENTS
  "main esptool_py driver lwip button logger lvgl mdns socket task wifi gui nvs ${HAL_COMPONENTS}"
  CACHE STRING
  "List of components to include"
  )

project(wireless-debug-display)

set(CMAKE_CXX_STANDARD 20)
