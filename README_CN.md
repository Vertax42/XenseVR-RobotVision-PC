# XenseVR-RobotVision-PC

PC 端配套程序,用于把 ZED 立体相机的画面实时推送到 Pico 4 Ultra Enterprise 上运行的 [`xense-vr-client`](https://github.com/Vertax42/xense-vr-client) APK。流程:用 Stereolabs SDK 抓 ZED 左/右目 → libx264(默认)或 NVENC 编码 → 4 字节 big-endian 长度前缀 + Annex-B NAL → TCP 发到头显的 `MediaDecoder` → Android `MediaCodec` 解码 → 上传到 Unity 纹理在头显双眼里渲染。

Fork 自 [XR-Robotics/XRoboToolkit-RobotVision-PC](https://github.com/XR-Robotics/XRoboToolkit-RobotVision-PC),协议 GPL-3.0。

> 英文文档见 [`README.md`](README.md)。

---

## 1. 硬件 / 软件要求

| 部件 | 要求 |
| --- | --- |
| PC 系统 | Ubuntu 22.04(20 估计也行,未实测),x86_64 |
| GPU | NVIDIA + NVENC(driver ≥ 525,如 RTX 30/40/50/PRO 系列) |
| 立体相机 | ZED SDK 5.x 支持的任一型号(ZED Mini / ZED 2 / ZED 2i / ZED X) |
| 头显 | Pico 4 Ultra Enterprise + [`xense-vr-client`](https://github.com/Vertax42/xense-vr-client) APK |
| 网络 | PC 和头显在同一 WiFi 子网,无端口阻断 |

PC 软件:

- conda / mamba 环境(如 `lerobot-xense`)里需要:
  - `ffmpeg 7.x`(带 NVENC),完整的 libav* 头(`libavcodec` / `libavformat` / `libavutil` / `libswscale` / `libavdevice`)
  - `cuda-toolkit 12.x`(`cuda-cudart-dev` + `cuda-nvcc`)
  - `gcc_linux-64` / `gxx_linux-64`(≥ 11)
- 系统 apt 包:
  - `libopencv-dev`
  - `libasio-dev`(可选,仓库自带 `asio-1.30.2/`)
- ZED SDK ≥ 5.3 装在 `/usr/local/zed/`

**不需要装系统 CUDA**(`/usr/local/cuda/` 可以不存在)——在激活的 conda env 里编译时,conda 编译器 wrapper 会把 `$CONDA_PREFIX/targets/x86_64-linux/{include,lib}` 自动塞进 `CPPFLAGS` / `LDFLAGS`。

---

## 2. 快速上手(Linux + conda)

### 2.1 安装 ZED SDK(只装一次)

去 <https://www.stereolabs.com/developers/release/> 下载对应版本,**激活 conda env 后再装**:

```bash
conda activate lerobot-xense
bash ZED_SDK_Ubuntu22_cuda12.8_*_v5.3.0.zstd.run -- --skip_od_module
```

`--skip_od_module` 跳过 AI 依赖(那一段会往 `/usr/local/cuda/lib64/` 复制 cuDNN/TensorRT,但我们不需要)。立体推流用不上 AI 模块。如果以后想用神经深度 / 物体检测 / 人体追踪,先装系统 CUDA 到 `/usr/local/cuda/`,再不带 `--skip_od_module` 重装一次。

ZED 安装器优先用 PATH 里的 `nvcc` 做版本检测,所以 conda env 里的 `$CONDA_PREFIX/bin/nvcc` 已经够,不需要系统 CUDA。

验证:

```bash
ls /usr/local/zed/lib/libsl_zed.so
python -c "import pyzed.sl as sl; print(sl.Camera.get_sdk_version())"
/usr/local/zed/tools/ZED_Diagnostic
```

### 2.2 克隆 + 编译

```bash
git clone git@github.com:Vertax42/XenseVR-RobotVision-PC.git
cd XenseVR-RobotVision-PC/VideoTransferPC/RobotVisionTest

conda activate lerobot-xense
make -f Makefile.orin -j$(nproc)
# 产物:./RobotVisionConsole
```

`Makefile.orin` 是通用 Linux Makefile(`.orin` 这个名字是上游 Jetson Orin port 的历史命名,**x86_64 也能直接用**)。Makefile 里的 `CPPFLAGS` / `CXXFLAGS` / `LDFLAGS` 用 `+=` 追加,所以 conda 编译器 wrapper 注入的路径会被保留。

### 2.3 检查产物

```bash
ldd ./RobotVisionConsole | grep -E "sl_zed|cudart|libcuda\.|avcodec|opencv"
./RobotVisionConsole          # 打印 CLI 帮助
```

应该看到 `libsl_zed.so → /usr/local/zed/lib/libsl_zed.so`、`libavcodec.so.61 → $CONDA_PREFIX/lib/...`、`libopencv_*.so → /lib/x86_64-linux-gnu/...`。

---

## 3. 测试流程

按四个 phase 顺序走,**不要跳**——每个 phase 都把下一层的可能故障隔离掉。

### Phase 1 — ZED 硬件 sanity(只用 pyzed)

```bash
conda activate lerobot-xense
python - <<'PY'
import pyzed.sl as sl
cam = sl.Camera()
init = sl.InitParameters()
init.camera_resolution = sl.RESOLUTION.HD720
init.camera_fps = 60
init.depth_mode = sl.DEPTH_MODE.NONE
err = cam.open(init); assert err == sl.ERROR_CODE.SUCCESS, err
info = cam.get_camera_information()
print(f"{info.camera_model}  S/N {info.serial_number}  FW {info.camera_configuration.firmware_version}")
mat = sl.Mat()
for _ in range(5):
    cam.grab(); cam.retrieve_image(mat, sl.VIEW.SIDE_BY_SIDE, sl.MEM.CPU)
    print(f"SBS {mat.get_width()}x{mat.get_height()}")
cam.close()
PY
```

期望:打出型号 + 序列号,然后 5 帧 `2560x720`(HD720 双目水平拼接)。

不通常见原因:USB 没插在 SuperSpeed(USB 3)口、用户不在 `plugdev` 组、ZED SDK 没装好。

### Phase 2 — PC 自闭环(ZED → 编 → 解,同进程)

```bash
cd VideoTransferPC/RobotVisionTest
./RobotVisionConsole --camera-test --width 1280 --height 720 --fps 60
# Q 退出。会持续打印 "Decoded frame size: 1382400 bytes, resolution: 1280x720"
# (1382400 = 1280*720*1.5 = LEFT 单目 YUV420 三平面)
```

验证:ZED 抓帧、libx264 编码、内存解码、callback 链。

### Phase 3 — TCP 自闭环(同机 server + client)

```bash
# 终端 A — server(收 NAL,解码后写 YUV)
./RobotVisionConsole --tcp-transfer s --ip 127.0.0.1 --port 12345

# 终端 B — client(ZED → 编 → TCP 发到 127.0.0.1)
./RobotVisionConsole --tcp-transfer c --ip 127.0.0.1 --port 12345 \
    --width 1280 --height 720 --fps 60
```

跑几秒 server 写出 `decoded_frames.yuv`,大小 `N × 1382400`。这条链路用的 wire format **跟头显侧 `MediaDecoder` 字节级一致**(4 字节 big-endian 长度前缀 + Annex-B NAL)。

### Phase 4 — 推到 Pico 头显(实战)

#### 头显侧
1. Pico 4 Ultra 接到和 PC 同一个 WiFi 子网,记下头显 IP。
2. 头显里启 `xense-vr-client` APK。
3. 进 Camera Ctrl 面板,toggle **Listen** 打开。
4. 弹对话框时,输入 **PC 在该子网的 IP**。

APK 在**端口 12345**(写死在 `xense-vr-client` 仓库 `Assets/Scripts/UI/UICameraCtrl.cs:39`)起一个 `MediaDecoder` TCP listener。

> **listener 是 single-accept**:一次连接一个会话。连接断了必须 toggle Listen 关再开重新 arm。**不要用 `nc` / `nmap` 探端口**——探针本身就会消耗那次 accept。

#### PC 侧

```bash
./RobotVisionConsole --tcp-camera c \
    --ip <HEADSET_IP> --port 12345 \
    --width 1280 --height 720 \
    --fps 60 --bitrate 4000000
```

这条命令用 ZED `sl::VIEW::SIDE_BY_SIDE`,编码器收到 `2560×720` 帧(HD720 横向拼接)。头显侧 `MediaCodec` 解出来,贴到 Unity `RawImage`,用户看到立体 3D。

如果 ZED 打开后立刻 `Fatal Error: Send error: Broken pipe`,看[第 6 节 — 已知问题](#6-已知问题)。

---

## 4. 通信协议

每个 H.264 NAL 单元在 TCP 上的封包格式:

```
| 4 字节 big-endian 长度(uint32 N)| N 字节的 Annex-B NAL 数据 |
```

Annex-B 起始码 `00 00 00 01` 包含在 N 字节里(libx264 设 `annexb=1` 自然就是这样)。

头显侧(`com.picovr.robotassistantlib.MediaDecoder`,在 `xense-vr-client` 的 AAR 里):

1. `ServerSocket.accept()` 一次
2. `DataInputStream.readFully(4)` → uint32 长度(BIG_ENDIAN)
3. `DataInputStream.readFully(length)` → NAL 数据
4. 把 NAL 喂给 Android `MediaCodec`(mime `video/avc`)
5. 循环 2-4,任何 I/O 错误 → 关 socket → 线程退出 → `ServerSocket.close()`

协议层**没有流控**。如果头显的 MediaCodec 输入队列堵住,我们的 `sendData` 最终会 broken pipe。

上游(也是这个 fork)用的编码器配置,头显 MediaCodec 必须接受:

| 参数 | 值 |
| --- | --- |
| MIME | `video/avc`(H.264) |
| Profile | Constrained Baseline |
| B 帧 | 0 |
| `annexb` | 1 |
| GOP | `2 × fps` |
| 像素格式 | `yuv420p` |

---

## 5. 切到 NVENC(40/50 系 / Blackwell 推荐)

`H264Encoder.cpp:27` 默认 `avcodec_find_encoder(AV_CODEC_ID_H264)`,选的是 libx264。换 NVENC:

```cpp
codec = avcodec_find_encoder_by_name("h264_nvenc");
if (!codec) {
    codec = avcodec_find_encoder(AV_CODEC_ID_H264);  // libx264 兜底
}
```

然后把 libx264 那段 `av_opt_set` 换成 NVENC 预设:

```cpp
av_opt_set     (m_encCtx->priv_data, "preset",      "p1",      0);  // p1 = 最快
av_opt_set     (m_encCtx->priv_data, "tune",        "ull",     0);  // 超低延迟
av_opt_set     (m_encCtx->priv_data, "rc",          "cbr",     0);
av_opt_set     (m_encCtx->priv_data, "zerolatency", "1",       0);
av_opt_set     (m_encCtx->priv_data, "profile",     "baseline",0);
av_opt_set_int (m_encCtx->priv_data, "delay",       0,         0);
```

更高分辨率的 SBS(2560×720@60 起)libx264 ultrafast 撑不到 60 fps(同进程 encode+decode 大约 25-30 fps),**这种情况 NVENC 是必须的**。

---

## 6. 已知问题

### 第一次 sendData 后立刻 `Fatal Error: Send error: Broken pipe`

头显的 MediaDecoder 在收到一两个 NAL 后就关闭 TCP。SPS/PPS/IDR 可能已经过 socket buffer,所以头显里能看到一闪而过的画面,然后断开。

可能原因(按优先级):

1. libx264 给 2560×720@60 选了 `Constrained Baseline level 4.2`。某些 Android `MediaCodec` 实现拿到 Level 4.2 SPS 会 throw。
2. MediaDecoder 的 MediaCodec 输入队列因为 Unity `RawImage` 那边消费太慢卡住。
3. WiFi RTT 抖动(实测 47–562 ms 区间)叠加 TCP 没有 ACK 缓冲就崩。

正在试的解决方向:

- 强制 `level=31`(Baseline 3.1)
- 把 bitrate 降到 2 Mbps,缩小 IDR 帧大小
- 缩 GOP(`gop=fps` 而不是 `2×fps`),让 SPS/PPS 更频繁出现
- 切 `h264_nvenc` 配 `delay=0`

要彻底定位,在头显里开 wireless ADB(开发者选项 → 无线调试),`adb connect <HEADSET_IP>:5555`,然后:

```bash
adb logcat -s MediaDecoder *:E
```

边推边看头显侧到底报什么错。

### Single-accept listener

`com.picovr.robotassistantlib.MediaDecoder` 每次 Listen 只 accept 一次连接。**探端口本身就会消耗那次 accept**。不要 `nc -vz <ip> 12345` 也不要 `nmap` 头显——直接 `RobotVisionConsole --tcp-camera c` 推流就行。要重新 arm:头显里 toggle Listen 关再开。

---

## 7. 仓库结构

### `VideoTransferPC/src/`

核心库(编进 `RobotVisionConsole`):

| 文件 | 作用 |
| --- | --- |
| `CameraCapture.cpp/h` | 通用相机抓帧(Windows 走 DirectShow;Linux 直接在 `main.cpp` 里用 ZED SDK) |
| `CameraDataSender.cpp/h` | 基于 asio 的 TCP 客户端,带 4 字节长度前缀 |
| `CameraDataReceiver.cpp/h` | 配套的 TCP 服务端 |
| `H264Encoder.cpp/h` | FFmpeg 编码器封装(默认 libx264) |
| `H264Decoder.cpp/h` | FFmpeg 解码器封装 |
| `H264NALUParser.cpp/h` | Annex-B NAL 解析工具 |
| `NetworkVideoSource.cpp/h` | 高层视频源,从网络拉编码帧 |
| `FFmpegUtils.cpp/h` | 杂项 FFmpeg helper |
| `SolidColorFrame.cpp/h`、`ColorFrameGenerator.cpp/h` | 合成测试图 |

### `VideoTransferPC/RobotVisionTest/`

`main.cpp` — 单二进制 CLI,暴露下面这些测试函数。
`Makefile.orin` — Linux Makefile(x86_64 和 Jetson Orin 都能用)。
`CMakeLists.txt` / `build_release.bat` — Windows / VS 构建路径(见第 8 节)。

### `VideoTransferPC/VideoPlayer/`

Qt 实现的 YUV 播放器(Windows 向)。头显这条路用不上,保留只为了和上游一致。

### `RobotVisionConsole` CLI 选项

| 选项 | 作用 |
| --- | --- |
| `--simple-capture` | ZED 抓帧 → 写裸 YUV 到 `captured_frames.yuv` |
| `--camera-test` | ZED → libx264 编 → 内存解码(自闭环) |
| `--tcp-transfer s` | TCP 服务端,接 NAL 流解码后写 `decoded_frames.yuv` |
| `--tcp-transfer c` | TCP 客户端,ZED LEFT 视图 → 编 → TCP 发 |
| `--tcp-camera c` | TCP 客户端,ZED `SIDE_BY_SIDE` → 编 → TCP 发(头显路径) |
| `--analyze-delay` | 读 `Messages.dblog`,输出延迟 CSV |

通用参数:`--ip <addr> --port <num> --width <px> --height <px> --fps <hz> --bitrate <bps>`。默认:`127.0.0.1:12345 1280×720@30 4Mbps`。

---

## 8. Windows 构建(legacy / 附录)

上游原本是 Windows + Visual Studio + Qt 路径,这条路也还在:

- Visual Studio 2019 Community
- Qt 6.6.x(MSVC 2019 64-bit) — 只有 `VideoPlayer` 用
- FFmpeg 7.1 — vendored(从上游的 `ffmpeg-7.1-full_build-shared/` 下载放过来)
- ASIO 1.30.2 — vendored

构建用 `VideoPlayer/build_release.bat` 和 `RobotVisionTest/build_release.bat`,产物 `RobotVisionConsole.exe` 和 `appVideoPlayer.exe`。

Linux x86_64 是现在推荐路径,见第 2 节。

---

## 9. 致谢

本项目 fork 自 [XRoboToolkit-RobotVision-PC](https://github.com/XR-Robotics/XRoboToolkit-RobotVision-PC)(GPL-3.0)。如果在学术工作里引用,请引用原始论文:

```
@article{zhao2025xrobotoolkit,
      title={XRoboToolkit: A Cross-Platform Framework for Robot Teleoperation},
      author={Zhigen Zhao and Liuchuan Yu and Ke Jing and Ning Yang},
      journal={arXiv preprint arXiv:2508.00097},
      year={2025}
}
```
