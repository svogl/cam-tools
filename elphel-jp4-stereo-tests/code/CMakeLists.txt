cmake_minimum_required(VERSION 2.8)

SET(CMAKE_MODULE_PATH "${PROJECT_SOURCE_DIR}/cmake_modules")

find_package( OpenCV REQUIRED )

add_executable( debayer debayer_test.cpp  )
target_link_libraries( debayer ${OpenCV_LIBS} )
