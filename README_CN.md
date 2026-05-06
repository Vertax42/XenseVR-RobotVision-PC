# RobotVision-PC 工程结构说明

## 1. 目录结构

### 1.1 src 目录（核心功能库）
包含视频处理的核心功能实现：

#### 1.1.1 视频捕获与处理
- `CameraCapture类`：摄像头捕获实现
- `CameraDataSender类`：摄像头数据发送
- `CameraDataReceiver类`：摄像头数据接收
- `VideoFrameProvider类`：播放器核心接口

#### 1.1.2 H.264 编解码
- `H264Encoder类`：H.264 编码器
- `H264Decoder类`：H.264 解码器
- `H264NALUParser类`：H.264 NALU 解析器

#### 1.1.3 网络传输
- `NetworkVideoSource类`：网络视频源实现

#### 1.1.4 工具类
- `SolidColorFrame类`：纯色帧
- `ColorFrameGenerator类`：纯色帧生成器

### 1.2 VideoPlayer 目录
实现了一个YUV视频播放器

### 1.3 RobotVisionTest 目录
RobotVisionTest 测试工程,用于对相关的功能类进行测试。

#### 1.3.1 主要功能模块
- 摄像头捕获和视频流处理
- H.264编码/解码
- TCP网络传输
- 时间戳处理
- 延迟分析

#### 1.3.2 测试函数

1. 摄像头捕获、H.264编码和解码测试
   - 函数：`RunCameraCaptureTest()`
   - 命令行选项：`--camera-test`
   - 功能：测试摄像头捕获、H.264编码和解码的完整流程
   - 处理流程：
     1. 摄像头捕获原始YUV帧
     2. 使用H.264编码器进行编码
     3. 使用H.264解码器进行解码
     4. 输出解码后的帧信息（大小和分辨率）
   - 使用示例：
     ```bash
     RobotVisionConsole.exe --camera-test
     ```

2. 简单摄像头捕获测试
   - 函数：`RunSimpleCameraCaptureTest()`
   - 命令行选项：`--simple-capture`
   - 功能：简单的摄像头捕获测试，将原始YUV帧保存到文件
   - 处理流程：
     1. 摄像头捕获原始YUV帧
     2. 直接将YUV数据写入文件
     3. 输出文件：captured_frames.yuv
   - 使用示例：
     ```bash
     RobotVisionConsole.exe --simple-capture
     ```

3. H.264 TCP传输测试
   - 函数：`runH264TCPTransferTest(int argc, char* argv[])`
   - 命令行选项：`--tcp-transfer [s|c]`
   - 参数说明：
     - s: 运行服务器端
     - c: 运行客户端
   - 功能：测试H.264编码后的TCP传输流程
   - 处理流程：
     1. 客户端：摄像头捕获 -> H.264编码 -> TCP发送
     2. 服务器端：TCP接收 -> H.264解码 -> 保存YUV文件
     3. 输出文件：decoded_frames.yuv
   - 使用示例：
     ```bash
     # 服务器端
     RobotVisionConsole.exe --tcp-transfer s
     # 客户端
     RobotVisionConsole.exe --tcp-transfer c
     ```

4. 完整摄像头捕获、H.264编码、TCP传输测试
   - 函数：`runH264TCPCameraCaptureTest(int argc, char* argv[])`
   - 命令行选项：`--tcp-camera [s|c]`
   - 参数说明：
     - s: 运行服务器端
     - c: 运行客户端
   - 功能：测试完整的摄像头捕获、H.264编码、TCP传输流程
   - 处理流程：
     1. 客户端：摄像头捕获 -> H.264编码 -> TCP发送
     2. 服务器端：TCP接收 -> H.264解码 -> 实时显示
     3. 支持连续捕获模式
   - 使用示例：
     ```bash
     # 服务器端
     RobotVisionConsole.exe --tcp-camera s
     # 客户端
     RobotVisionConsole.exe --tcp-camera c
     ```

5. 延迟分析测试
   - 函数：`analyzeDelay(int argc, char* argv[])`
   - 命令行选项：`--analyze-delay`
   - 功能：分析视频处理各阶段的延迟
   - 处理流程：
     1. 读取日志文件（Messages.dblog）
     2. 分析6个处理阶段的延迟：
        - 摄像头捕获
        - YUV420转换
        - H.264编码
        - TCP传输
        - H.264解码
        - 帧显示
     3. 生成CSV格式的延迟分析报告（output.csv）
   - 使用示例：
     ```bash
     RobotVisionConsole.exe --analyze-delay
     ```

## 2. 构建说明
项目使用 CMake 构建系统，主要包含两个可执行文件：
1. VideoPlayer：视频播放器应用
2. RobotVisionTest：测试工程

构建依赖：
- Qt 框架
- FFmpeg 库
- CMake 3.10 或更高版本
- C++17 或更高版本编译器
- ASIO 库（用于网络通信）

## 3. 构建方法

### 3.1 环境配置
构建前需要确保以下环境已正确配置：
- Visual Studio 2019 Community（位于 `D:\MyProgram\Microsoft Visual Studio\2019\Community`）
- Qt 6.6.2（位于 `D:\MyProgram\Qt6\6.6.2\msvc2019_64`）
- FFmpeg 7.1（位于项目根目录的 `../../ffmpeg-7.1-full_build-shared`）
- ASIO 1.30.2（位于项目根目录的 `../../asio-1.30.2`）

### 3.2 VideoPlayer 构建步骤
VideoPlayer 是基于 Qt 的视频播放器应用，可以通过以下方式构建：

#### 3.2.1 使用构建脚本
项目提供了自动构建脚本 `VideoPlayer\build_release.bat`，执行此脚本将自动完成以下步骤：
1. 创建 `build_release` 目录
2. 配置 Qt 环境
3. 使用 CMake 生成 Visual Studio 项目文件
4. 编译项目生成可执行文件

构建完成后，可执行文件位于 `VideoPlayer\build_release\Release\appVideoPlayer.exe`

### 3.3 RobotVisionTest 构建步骤
RobotVisionTest 是用于测试视频处理功能的控制台应用，可以通过以下方式构建：

#### 3.3.1 使用构建脚本
项目提供了自动构建脚本 `RobotVisionTest\build_release.bat`，执行此脚本将自动完成以下步骤：
1. 创建 `build_release` 目录
2. 配置 FFmpeg 和 ASIO 环境
3. 使用 CMake 生成 Visual Studio 项目文件
4. 编译项目生成可执行文件

构建完成后，可执行文件位于 `RobotVisionTest\build_release\Release\RobotVisionConsole.exe`

