cmake_minimum_required(VERSION 3.16)

project(VideoPlayer VERSION 0.1 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD_REQUIRED ON)

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

find_package(Qt6 6.4 REQUIRED COMPONENTS Quick Multimedia)

qt_standard_project_setup()

qt_add_executable(appVideoPlayer
    main.cpp
    ../src/NetworkVideoSource.cpp
    ../src/CameraDataReceiver.cpp
    ../src/H264Decoder.cpp
    ../src/FFmpegUtils.cpp
)

target_include_directories(appVideoPlayer PRIVATE
    ${FFMPEG_INCLUDE_DIR}
    ${ASIO_INCLUDE_DIR}
    ${CMAKE_SOURCE_DIR}/../src
)

target_link_directories(appVideoPlayer PRIVATE ${FFMPEG_LIB_DIR})

qt_add_qml_module(appVideoPlayer
    URI VideoPlayer
    VERSION 1.0
    QML_FILES Main.qml
    SOURCES ../src/VideoFrameProvider.cpp
    SOURCES ../src/ColorFrameGenerator.cpp
    SOURCES ../src/SolidColorFrame.cpp
    QML_FILES VideoPlayer.qml
)

# Qt for iOS sets MACOSX_BUNDLE_GUI_IDENTIFIER automatically since Qt 6.1.
# If you are developing for iOS or macOS you should consider setting an
# explicit, fixed bundle identifier manually though.
set_target_properties(appVideoPlayer PROPERTIES
#    MACOSX_BUNDLE_GUI_IDENTIFIER com.example.appVideoPlayer
    MACOSX_BUNDLE_BUNDLE_VERSION ${PROJECT_VERSION}
    MACOSX_BUNDLE_SHORT_VERSION_STRING ${PROJECT_VERSION_MAJOR}.${PROJECT_VERSION_MINOR}
    MACOSX_BUNDLE TRUE
    WIN32_EXECUTABLE TRUE
)

target_link_libraries(appVideoPlayer
    PRIVATE Qt6::Quick
    PRIVATE Qt6::Multimedia
    avcodec
    avformat
    avdevice
    swscale
    avutil
)

include(GNUInstallDirs)
install(TARGETS appVideoPlayer
    BUNDLE DESTINATION .
    LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR}
    RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR}
)
