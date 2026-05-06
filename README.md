# XenseVR-RobotVision-PC Project Structure

## 1. Directory Structure

### 1.1 src Directory (Core Function Library)
Contains core video processing functionality implementations:

#### 1.1.1 Video Capture and Processing
- `CameraCapture class`: Camera capture implementation
- `CameraDataSender class`: Camera data transmission
- `CameraDataReceiver class`: Camera data reception
- `VideoFrameProvider class`: Player core interface

#### 1.1.2 H.264 Codec
- `H264Encoder class`: H.264 encoder
- `H264Decoder class`: H.264 decoder
- `H264NALUParser class`: H.264 NALU parser

#### 1.1.3 Network Transmission
- `NetworkVideoSource class`: Network video source implementation

#### 1.1.4 Utility Classes
- `SolidColorFrame class`: Solid color frame
- `ColorFrameGenerator class`: Solid color frame generator

### 1.2 VideoPlayer Directory
Implements a YUV video player

### 1.3 RobotVisionTest Directory
RobotVisionTest project for testing related functional classes.

#### 1.3.1 Main Functional Modules
- Camera capture and video stream processing
- H.264 encoding/decoding
- TCP network transmission
- Timestamp processing
- Latency analysis

#### 1.3.2 Test Functions

1. Camera Capture, H.264 Encoding and Decoding Test
   - Function: `RunCameraCaptureTest()`
   - Command line option: `--camera-test`
   - Functionality: Tests complete camera capture, H.264 encoding and decoding process
   - Process flow:
     1. Camera captures raw YUV frames
     2. Uses H.264 encoder for encoding
     3. Uses H.264 decoder for decoding
     4. Outputs decoded frame information (size and resolution)
   - Usage example:
     ```bash
     RobotVisionConsole.exe --camera-test
     ```

2. Simple Camera Capture Test
   - Function: `RunSimpleCameraCaptureTest()`
   - Command line option: `--simple-capture`
   - Functionality: Simple camera capture test, saves raw YUV frames to file
   - Process flow:
     1. Camera captures raw YUV frames
     2. Directly writes YUV data to file
     3. Output file: captured_frames.yuv
   - Usage example:
     ```bash
     RobotVisionConsole.exe --simple-capture
     ```

3. H.264 TCP Transfer Test
   - Function: `runH264TCPTransferTest(int argc, char* argv[])`
   - Command line option: `--tcp-transfer [s|c]`
   - Parameter description:
     - s: Run server side
     - c: Run client side
   - Functionality: Tests H.264 encoded TCP transmission process
   - Process flow:
     1. Client: Camera capture -> H.264 encoding -> TCP transmission
     2. Server: TCP reception -> H.264 decoding -> Save YUV file
     3. Output file: decoded_frames.yuv
   - Usage example:
     ```bash
     # Server side
     RobotVisionConsole.exe --tcp-transfer s
     # Client side
     RobotVisionConsole.exe --tcp-transfer c
     ```

4. Complete Camera Capture, H.264 Encoding, TCP Transmission Test
   - Function: `runH264TCPCameraCaptureTest(int argc, char* argv[])`
   - Command line option: `--tcp-camera c`
   - Parameter description:
     - c: Run client side
   - Functionality: Tests complete camera capture, H.264 encoding, TCP transmission process
   - Process flow:
     1. Client: Camera capture -> H.264 encoding -> TCP transmission
     2. Server: TCP reception -> H.264 decoding -> Real-time display
     3. Supports continuous capture mode
   - Usage example:
     1. PC (ZED) -> PC
     ```bash
     # Server side
     appVideoPlayer.exe --ip {LOCAL_IP} --port 12345
     # Client side
     RobotVisionConsole.exe --tcp-camera c
     ```
     2. PC (ZED) - VR
     ```
     # Headset side
     Open xense-vr-client Unity App, Select ZEDMINI, Click Listen
     # PC side
     RobotVisionConsole.exe --tcp-camera -c --ip {HEADSET_IP} --port 12345 --width 1280 --height 720 --fps 60 --bitrate 4000000
     ```


5. Latency Analysis Test
   - Function: `analyzeDelay(int argc, char* argv[])`
   - Command line option: `--analyze-delay`
   - Functionality: Analyzes latency at various stages of video processing
   - Process flow:
     1. Reads log file (Messages.dblog)
     2. Analyzes latency for 6 processing stages:
        - Camera capture
        - YUV420 conversion
        - H.264 encoding
        - TCP transmission
        - H.264 decoding
        - Frame display
     3. Generates CSV format latency analysis report (output.csv)
   - Usage example:
     ```bash
     RobotVisionConsole.exe --analyze-delay
     ```

## 2. Build Instructions
The project uses CMake build system and mainly contains two executables:
1. VideoPlayer: Video player application
2. RobotVisionTest: Test project

Build dependencies:
- Qt framework
- FFmpeg library
- CMake 3.10 or higher
- C++17 or higher compiler
- ASIO library (for network communication)

## 3. Build Methods

### 3.1 Environment Configuration
Before building, ensure the following environment is correctly configured:
- [Visual Studio 2019 Community](https://ia804501.us.archive.org/6/items/vs_Community/vs_Community.exe) (located at `D:\MyProgram\Microsoft Visual Studio\2019\Community`)
- Qt 6.6.2 (located at `D:\MyProgram\Qt6\6.6.2\msvc2019_64`)

    <details>
    <summary>Installation Guidance</summary>

    - Install [Qt Online Installer](https://doc.qt.io/qt-6/qt-online-installation.html)
    - Select Custom Installation
    - Select 
        - Qt 6.6.x
            - MSVC 2019 64-bit
            - Qt Quick 3D
			- Qt 5 Compatibility Module
            - Qt Shader Tools
            - Additional Libraries
                - Qt 3D
                - Qt Charts
                - Qt Graphs (TP)
                - Qt Multimedia
                - Qt Positioning
                - Qt Quick 3D Physics
                - Qt Quick Effect Maker
                - Qt WebChannel
                - Qt WebEngine
                - Qt WebSockets
                - Qt WebView
            - Qt Quick TimeLine
        - Build Tools
            - CMake
            - Ninja
    - Install
    </details>

- FFmpeg 7.1 (located at project root directory `../../ffmpeg-7.1-full_build-shared`)
- ASIO 1.30.2 (located at project root directory `../../asio-1.30.2`)

### 3.2 VideoPlayer Build Steps
VideoPlayer is a Qt-based video player application that can be built in the following ways:

#### 3.2.1 Using Build Script
The project provides an automatic build script `VideoPlayer\build_release.bat`. Executing this script will automatically complete the following steps:
1. Create `build_release` directory
2. Configure Qt environment
3. Generate Visual Studio project files using CMake
4. Compile project to generate executable

After building, the executable will be located at `VideoPlayer\build_release\Release\appVideoPlayer.exe`

### 3.3 RobotVisionTest Build Steps
RobotVisionTest is a console application for testing video processing functionality that can be built in the following ways:

#### 3.3.1 Using Build Script
The project provides an automatic build script `RobotVisionTest\build_release.bat`. Executing this script will automatically complete the following steps:
1. Create `build_release` directory
2. Configure FFmpeg and ASIO environment
3. Generate Visual Studio project files using CMake
4. Compile project to generate executable

After building, the executable will be located at `RobotVisionTest\build_release\Release\RobotVisionConsole.exe` 

## Notes

1. ZED sdk is required to compile the VideoTransferPC program.

2. For the windows version

- It requires **Visual Studio 2019**.

- The VideoPlayer project depends on **Qt 6.6.3**, see the `build_release.bat` file for more details.

## Acknowledgements

This project is forked from [XRoboToolkit-RobotVision-PC](https://github.com/XR-Robotics/XRoboToolkit-RobotVision-PC) (GPL-3.0). If you use the underlying methodology in academic work, please cite the original paper:

```
@article{zhao2025xrobotoolkit,
      title={XRoboToolkit: A Cross-Platform Framework for Robot Teleoperation}, 
      author={Zhigen Zhao and Liuchuan Yu and Ke Jing and Ning Yang}, 
      journal={arXiv preprint arXiv:2508.00097},
      year={2025}
}
```
