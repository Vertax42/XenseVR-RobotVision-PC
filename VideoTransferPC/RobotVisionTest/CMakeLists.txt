cmake_minimum_required(VERSION 3.14)

project(RobotVisionConsole LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
SET(CMAKE_BUILD_TYPE "Release") # Added from ZED snippet

# Add Windows version definition
if(WIN32)
    add_definitions(-D_WIN32_WINNT=0x0601)
endif()

# Set FFmpeg path
set(FFMPEG_ROOT "../../ffmpeg-7.1-full_build-shared")
set(FFMPEG_INCLUDE_DIR "${FFMPEG_ROOT}/include")
set(FFMPEG_LIB_DIR "${FFMPEG_ROOT}/lib")

# Set asio path
set(ASIO_ROOT "${CMAKE_SOURCE_DIR}/../../asio-1.30.2/include")
set(ASIO_INCLUDE_DIR "${ASIO_ROOT}")

# ZED SDK Integration
option(LINK_SHARED_ZED "Link with the ZED SDK shared executable" ON)
if (NOT LINK_SHARED_ZED AND MSVC)
    message(FATAL_ERROR "LINK_SHARED_ZED OFF : ZED SDK static libraries not available on Windows")
endif()

find_package(ZED REQUIRED)
find_package(OpenCV REQUIRED)
find_package(CUDA ${ZED_CUDA_VERSION} REQUIRED)

include_directories(${CUDA_INCLUDE_DIRS})
include_directories(${ZED_INCLUDE_DIRS})
include_directories(${OpenCV_INCLUDE_DIRS})
include_directories(${CMAKE_CURRENT_SOURCE_DIR}/include)

link_directories(${ZED_LIBRARY_DIR})
link_directories(${OpenCV_LIBRARY_DIRS})
link_directories(${CUDA_LIBRARY_DIRS})

add_executable(RobotVisionConsole
  main.cpp
  ../src/CameraCapture.cpp
  ../src/CameraDataReceiver.cpp
  ../src/CameraDataSender.cpp
  ../src/H264Decoder.cpp
  ../src/H264Encoder.cpp
  ../src/FFmpegUtils.cpp
  ../src/H264NALUParser.cpp
  ../src/NetworkVideoSource.cpp
)

# Use modern way to set include directories and library directories
target_include_directories(RobotVisionConsole PRIVATE
    ${FFMPEG_INCLUDE_DIR}
    ${ASIO_INCLUDE_DIR}
    ${CMAKE_SOURCE_DIR}/../src
    ${CUDA_INCLUDE_DIRS} # Added for ZED
    ${ZED_INCLUDE_DIRS} # Added for ZED
    ${CMAKE_CURRENT_SOURCE_DIR}/include # Added for ZED (adjust if your project has a different include structure)
)

target_link_directories(RobotVisionConsole PRIVATE
    ${FFMPEG_LIB_DIR}
    ${ZED_LIBRARY_DIR} # Added for ZED
    ${CUDA_LIBRARY_DIRS} # Added for ZED
)

# Link FFmpeg libraries
target_link_libraries(RobotVisionConsole
    avcodec
    avformat
    avdevice
    swscale
    avutil
)

# Link ZED SDK libraries
if (LINK_SHARED_ZED)
    SET(ZED_LIBS ${ZED_LIBRARIES} ${CUDA_CUDA_LIBRARY} ${CUDA_CUDART_LIBRARY})
else()
    SET(ZED_LIBS ${ZED_STATIC_LIBRARIES} ${CUDA_CUDA_LIBRARY} ${CUDA_LIBRARY})
endif()
target_link_libraries(RobotVisionConsole ${ZED_LIBS} ${OpenCV_LIBRARIES})

# You might also need OpenCV if your C++ code uses it for image processing (e.g., RGBA to YUV conversion)
# find_package(OpenCV REQUIRED) # Uncomment if you use OpenCV
# target_link_libraries(RobotVisionConsole ${OpenCV_LIBRARIES}) # Uncomment if you use OpenCV

add_definitions(-g) # Added from ZED snippet

include(GNUInstallDirs)
install(TARGETS RobotVisionConsole
    LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR}
    RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR}
)