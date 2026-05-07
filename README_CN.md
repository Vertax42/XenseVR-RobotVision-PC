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

### 1.1 依赖分层

```
┌──────────────────────────────────────────────────────────┐
│  系统层(apt / driver)                                  │
│  ├── NVIDIA driver           ≥ 525(提供 libcuda)         │
│  ├── libopencv-dev           (系统 multi-arch)            │
│  ├── libasio-dev             (可选,仓库自带)              │
│  └── libavcodec-dev 等       (可选,被 conda 顶替)         │
├──────────────────────────────────────────────────────────┤
│  conda env(如 lerobot-xense)                            │
│  ├── ffmpeg 7.x + 完整 libav* + nvenc                    │
│  ├── cuda-toolkit 12.x + nvcc + cudart                   │
│  └── gcc/gxx ≥ 11 + sysroot                              │
├──────────────────────────────────────────────────────────┤
│  /usr/local/zed/   (Stereolabs SDK 安装目录)             │
│  ├── lib/libsl_zed.so   (~168 MB,dlopen libcuda.so.1)    │
│  ├── tools/{ZED_Diagnostic, ZED_Explorer, ...}           │
│  └── samples/                                            │
└──────────────────────────────────────────────────────────┘
```

表里**没显式列**的隐藏依赖:

- `libsl_zed.so` 自己 `dlopen` `libcuda.so.1`,这个是 **NVIDIA driver 提供的**,不是 SDK 提供。要先有 NVIDIA driver 才能跑 ZED。
- `libsl_zed.so` 直接用 CUDA driver API,**不依赖** `/usr/local/cuda/` 的存在(只要驱动给 libcuda 就行)。

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

#### 安装器内部到底干了什么

```
bash ZED_SDK_*.run -- --skip_od_module
  ↓ makeself 解压(zstd,~3 GB)→ /tmp/selfgz_xxx/
  ↓ exec ./linux_install_release.sh
    ↓ check_cuda_version()
    │   优先级:command -v nvcc → /usr/local/cuda/bin/nvcc → /usr/local/cuda/version.txt
    │   命中 conda env nvcc 12.8 → "OK: Found CUDA 12.8"
    ↓ check_nvidia_driver() —— nvidia-smi 探测,任何输出就算过
    ↓ 提示输入安装路径(默认 /usr/local/zed,直接回车)
    ↓ sudo cp -r lib/*.so* include/* zed-config*.cmake → /usr/local/zed/
    ↓ sudo cp -r tools/* → /usr/local/zed/tools/(可选)
    ↓ AI 模块复制(被 --skip_od_module 跳过)
    ↓ apt install libjpeg-turbo8 libusb-1.0-0 libopenblas-dev mesa-utils ...
    ↓ udev 规则放到 /lib/udev/rules.d/(不是 /etc/udev/rules.d/)
    ↓ pyzed wheel 安装到 sys.executable 对应的 site-packages
    ↓ sysctl net.core.rmem_max/default = 1048576(给 Stereolabs 自家 streaming 协议用)
```

**重入安全**:`.run` 文件可以反复跑——它检测到已安装就提示覆盖。默认安装路径 `/usr/local/zed/` 写死在 `Makefile.orin` 里,不要改路径(除非你也改 Makefile)。

#### 验证

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

#### `make` 实际做什么

每个 `.cpp` 编译时实际命令:

```
$(CXX) $(CPPFLAGS) $(CXXFLAGS) -c file.cpp -o file.o
```

`$(CXX) = g++` 在 Makefile 第 9 行**硬编码**,不从 env 取。这是**故意的**——conda 的 wrapper(`$CONDA_PREFIX/bin/x86_64-conda-linux-gnu-c++`)的 sysroot 不搜系统 `/usr/include`,会让我们找不到系统 OpenCV。

`$CPPFLAGS` 实际值(激活 conda env 时):

```
[conda env 注入]
  -DNDEBUG -D_FORTIFY_SOURCE=2 -O2
  -isystem $CONDA_PREFIX/include
  -I$CONDA_PREFIX/targets/x86_64-linux/include    ← cuda.h, cuda_runtime.h
[Makefile 追加]
  -std=c++17
  -I../../asio-1.30.2/include    ← 仓库自带 asio
  -I../src
  -I/usr/local/zed/include       ← Camera.hpp
  -I/usr/include/opencv4         ← 系统 OpenCV
  -I/usr/local/cuda/include      ← 不存在,gcc 静默忽略
```

`$LDFLAGS` 实际值:

```
[conda env 注入]
  -Wl,-rpath,$CONDA_PREFIX/lib
  -Wl,-rpath-link,$CONDA_PREFIX/lib
  -L$CONDA_PREFIX/lib                              ← libavcodec.so
  -L$CONDA_PREFIX/targets/x86_64-linux/lib         ← libcudart.so
  -L$CONDA_PREFIX/targets/x86_64-linux/lib/stubs   ← libcuda.so 的 stub
  + (其他 hardening flags)
[Makefile 追加]
  -L/usr/local/zed/lib                             ← libsl_zed.so
  -L/usr/local/cuda/lib64                          ← 不存在,无害
  -L/usr/lib/x86_64-linux-gnu                      ← 系统 OpenCV
  + -lavcodec -lavformat -lavutil -lswscale -lavdevice -lsl_zed -lcuda -lcudart -lopencv_*  -lpthread
```

#### 运行时 lib 解析(`ldd` 验证过的)

```
libavcodec.so.61   → $CONDA_PREFIX/lib/libavcodec.so.61   (rpath)
libsl_zed.so       → /usr/local/zed/lib/libsl_zed.so       (-L)
libopencv_*.so     → /lib/x86_64-linux-gnu/libopencv_*.so  (-L)
libcuda.so.1       → /lib/x86_64-linux-gnu/libcuda.so.1    (NVIDIA driver 提供)
libcudart.so.12    → $CONDA_PREFIX/.../libcudart.so.12     (rpath)
```

所以**激活 conda env 不是必须的运行时条件**——rpath 已经把 conda 路径烧进二进制了,只要 `lerobot-xense` env 还在原地就能跑。但**编译时必须激活**(否则 `$CPPFLAGS` 不注入,会因为找不到 `cuda.h` 编译失败)。

### 2.3 检查产物

```bash
ldd ./RobotVisionConsole | grep -E "sl_zed|cudart|libcuda\.|avcodec|opencv"
./RobotVisionConsole          # 打印 CLI 帮助
```

---

## 3. CLI 参考

`RobotVisionConsole` 一共 6 个子命令模式。共用以下参数(默认值是上游 Windows DirectShow 时代留下的):

| 参数 | 默认 | 说明 |
| --- | --- | --- |
| `--ip <addr>` | `127.0.0.1` | TCP 目标 IP(client)/ 绑定 IP(server) |
| `--port <num>` | `12345` | TCP 端口 |
| `--width <px>` | `1280` | 编码器输入宽度(单目;`--tcp-camera c` 内部会乘 2) |
| `--height <px>` | `720` | 编码器输入高度 |
| `--fps <hz>` | `30` | 抓帧 / 编码 fps |
| `--bitrate <bps>` | `4000000` | 编码器码率,**只有 `--tcp-camera c` 用** |
| `--camera <str>` | `video=Integrated Webcam` | **Linux 上完全忽略**(Windows DirectShow 残留) |

### 3.1 ZED 分辨率分发

`--width × --height` 在 `main.cpp:158-179`(以及其他几个测试函数里的拷贝)按硬编码白名单匹配 ZED 原生模式:

| `--width × --height` | ZED 模式 | 单目 | SBS(= 2W × H) |
| --- | --- | --- | --- |
| `1920 × 1080` | `HD1080` | 1920×1080 | 3840×1080 |
| `1280 × 720` | `HD720` | 1280×720 | 2560×720 |
| `2208 × 1242` | `HD2K` | 2208×1242 | 4416×1242 |
| `672 × 376` | `VGA` | 672×376 | 1344×376 |
| 其他 | 落 `HD720` + warning | 1280×720 | 2560×720 |

自定义分辨率(如 `2160×1440`)**会静默降级到 HD720**。要支持其他分辨率得改 `main.cpp` 里的分发表。

### 3.2 各模式

| 模式 | 函数 | ZED 视图 | 编码器输入 | 网络角色 | 输出 |
| --- | --- | --- | --- | --- | --- |
| `--simple-capture` | `RunSimpleCameraCaptureTest` | `SIDE_BY_SIDE` | n/a(直接落盘) | 无 | `captured_frames.yuv` |
| `--camera-test` | `RunCameraCaptureTest` | `LEFT` | `W × H` | 自闭环(同进程) | stdout |
| `--tcp-transfer s` | `runH264TCPTransferTest("s")` | —(不开相机) | — | TCP server | `decoded_frames.yuv` |
| `--tcp-transfer c` | `runH264TCPTransferTest("c")` | `LEFT` | `W × H` | TCP client | 网络发送 |
| `--tcp-camera c` | `runH264TCPCameraCaptureTest` | `SIDE_BY_SIDE` | `2W × H` ⚠️ | TCP client | 网络发送 |
| `--analyze-delay` | `analyzeDelay` | — | — | 无 | 从 `Messages.dblog` 生成 `output.csv` |

⚠️ **只有 `--tcp-camera c` 在 H264Encoder 构造时把宽度乘 2**(`H264Encoder(W*2, H, ...)`)。其他取 `SIDE_BY_SIDE` 视图的模式(如 `--simple-capture`)按实际 2W 直接落盘,不再乘 2。

### 3.3 怎么停止

代码里的"按 Q 退出"用的 `_kbhit() / _getch()`(Linux 用 termios + `O_NONBLOCK` mock),**只在交互终端有效**。下面这些场景 Q 全部不工作:

- 输出重定向到文件 / 管道
- 后台运行(`nohup`、有时 `screen`/`tmux`)
- stdin 关闭 / `</dev/null`

这种时候只能 `Ctrl+C`(SIGINT)、`kill <pid>` 或 `timeout N command`。代码**没有 SIGINT/SIGTERM handler**,所以信号杀进程时 `zed.close()` 不会被调用——等几秒再重新 open ZED,让内核释放 USB。

### 3.4 sender 和 receiver 的内部行为

**Sender**(`CameraDataSender`,`--tcp-transfer c` 和 `--tcp-camera c` 都用):

```c++
sender.startConnect(run);   // async_connect — 不阻塞,成功失败都进 callback
sender.runIOContext();      // io_context.run() — socket 关之前一直跑
```

`startConnect` **立刻返回**,真正的 `run` lambda(开 ZED + 编 + 发)在 asio 的 callback 里触发——**连接成功才进 callback**。这就是为什么 connect 失败时仍然能看到 "ZED Camera successfully opened" 后才 broken pipe——run 已经被 callback 启动了。

`sendData` 走**同步阻塞** `m_socket.send(...)`,失败抛 `SendDataException`,被 `printErrorAndQuit` 捕获后调 `std::exit(EXIT_FAILURE)`(整个进程死)。

**Receiver**(`CameraDataReceiver`,只有 `--tcp-transfer s` 用):

`acceptConnections()` 在每个客户端断开后**递归调自己**重新 accept(`CameraDataReceiver.cpp:64`):

```c++
if (!m_should_exit) {
    acceptConnections();   // 接下一个客户端
}
```

所以 `--tcp-transfer s` 是**多次接受**的,可以反复连客户端测试不用重启 server。**这跟头显侧不一样**:头显里的 `com.picovr.robotassistantlib.MediaDecoder` 是**单次 accept**——一次 Listen 点击 → 一次 `ServerSocket.accept()` → 一个会话 → 会话结束自动 `ServerSocket.close()`。

---

## 4. 测试流程

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

如果 ZED 打开后立刻 `Fatal Error: Send error: Broken pipe`,看[第 8 节 — 已知问题](#8-已知问题)。

### 4.1 常用命令场景

#### 新机器初次部署

```bash
git clone git@github.com:Vertax42/XenseVR-RobotVision-PC.git ~/XenseVR-RobotVision-PC
cd ~/XenseVR-RobotVision-PC
bash ZED_SDK_*.run -- --skip_od_module        # 假设你已经下载好了
conda activate lerobot-xense
cd VideoTransferPC/RobotVisionTest
make -f Makefile.orin -j$(nproc)              # 产物 ./RobotVisionConsole
```

#### 改了源码后重新编译

```bash
conda activate lerobot-xense
cd ~/XenseVR-RobotVision-PC/VideoTransferPC/RobotVisionTest
make -f Makefile.orin -j$(nproc)              # 增量
# 或
make -f Makefile.orin clean && make -f Makefile.orin -j$(nproc)   # 全量
```

#### 录一段 ZED 流到本地做离线回放

```bash
./RobotVisionConsole --simple-capture --width 1280 --height 720 --fps 60
# 生成 captured_frames.yuv,SBS YUV420(2560 × 720)
# 用 ffplay 看:
ffplay -f rawvideo -pixel_format yuv420p -video_size 2560x720 -framerate 60 captured_frames.yuv
```

(注意 8.3 节的 `--simple-capture` GPU/CPU 内存 bug)

#### 收尾清理

```bash
make -f Makefile.orin clean    # 删 .o 和二进制
rm -f captured_frames.yuv decoded_frames.yuv frame.jpg Messages.dblog output.csv
```

---

## 5. 通信协议

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

### 5.1 字节级握手(完整一次会话)

```
TCP 三次握手
  PC → Pico: SYN
  Pico → PC: SYN-ACK   (Pico 的 ServerSocket.accept() 返回 Socket)
  PC → Pico: ACK

[NALU 1 — 通常是 SPS + PPS + SEI 合并包,~200-300 B]
  PC 写: 4-byte BE length=0x000000C8 + 0x00 0x00 0x00 0x01 0x67 (SPS) ...
                                       0x00 0x00 0x00 0x01 0x68 (PPS) ...
                                       0x00 0x00 0x00 0x01 0x06 (SEI) ...
  Pico 读: readFully(4) → length=200, readFully(200) → payload
  Pico decode(): MediaCodec.configure(csd-0=SPS, csd-1=PPS),不出帧

[NALU 2 — IDR 关键帧,~70 KB at 4 Mbps × GOP=120]
  PC 写: length=0x000114A4 + 00 00 00 01 65 (IDR) + slice bytes ...
  Pico 读: readFully(4)=70820, readFully(70820)
  Pico decode(): MediaCodec 解码 → 输出帧到 Surface → Unity 纹理更新

[后续 P 帧,每帧 ~5-10 KB]
  ... 直到 broken pipe / Q / GOP 边界(120 帧后又一个 IDR)
```

### 5.2 编码器配置(`H264Encoder.cpp:67-94`)

| 参数 | 值 | 说明 |
| --- | --- | --- |
| MIME | `video/avc`(H.264) | `AV_CODEC_ID_H264` |
| Profile | Constrained Baseline | 无 CABAC,无 B 帧 |
| `max_b_frames` | `0` | 解码端不需要重排 |
| `annexb` | `1` | 字节流里出现 `00 00 00 01` 起始码而不是 length-prefix |
| `gop_size` | `2 × fps` | 2 秒一个 IDR |
| `pix_fmt` | `yuv420p` | |
| `preset` | `ultrafast` | libx264 |
| `tune` | `zerolatency` | libx264 |
| `sc_threshold` | `0` | 关闭场景切换检测,GOP 完全由 `gop_size` 决定 |

`annexb=1` 让 libx264 输出 `00 00 00 01` 起始码。**这是 MediaCodec 兼容性的关键**——头显的 `MediaDecoder` 靠起始码定位 csd-0 / csd-1。改成 `0` 头显侧 MediaCodec 配置会失败(且失败可能是静默的)。

---

## 6. 切到 NVENC

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

## 7. analyze-delay(离线时延工具)

`--analyze-delay` **不是**做实时 profiling 的——它是**离线日志分析器**,需要先有一份 `Messages.dblog`,里面包含**特定字符串 marker**。

### 7.1 期望的日志格式

```
<seq:int> <timestamp:float seconds> <pid:int> <procName:string> <message...>
```

例:

```
1 1736400000.123456 12345 RobotVisionConsole get frame from camera
2 1736400000.124000 12345 RobotVisionConsole get yuv420 frame
3 1736400000.125000 12345 RobotVisionConsole encode done send size 8123
4 1736400000.130000 12345 RobotVisionConsole dataCallback decode input size 8123
5 1736400000.131000 12345 RobotVisionConsole frameCallback decode output size 1382400
6 1736400000.132000 12345 RobotVisionConsole presentFrame done
```

6 个 stage marker(消息里**字面包含**这些字符串才被识别):

| Stage | 字符串 marker | 含义 |
| --- | --- | --- |
| 0 | `get frame from camera` | ZED `grab()` 完成 |
| 1 | `get yuv420 frame` | RGBA→YUV 转换完成 |
| 2 | `encode done send size` | libx264 编完一帧 |
| 3 | `dataCallback decode input size` | 接收端拿到完整 NALU |
| 4 | `frameCallback decode output size` | 解码器吐出 YUV |
| 5 | `presentFrame done` | 显示完成 |

输出 `output.csv`:

```
Capture,Encode,tcp transfer,Decode,Present
0.544,1.000,5.000,1.000,1.000
...
```

每行一个完整 5 段延迟(ms),最后一行是各列平均值。

### 7.2 注意 — instrumentation 还没接

**当前代码里没有任何地方打印这些 marker**。要让 `--analyze-delay` 真正能用,你得自己在抓帧循环、编码器、receiver、显示端各加 log,并都按上述格式写到 `Messages.dblog`。这是一个**未完成的框架**——分析器写好了,但日志生产端还没接。

---

## 8. 已知问题

### 8.1 第一次 sendData 后立刻 `Fatal Error: Send error: Broken pipe`

头显的 MediaDecoder 在收到一两个 NAL 后就关闭 TCP。SPS/PPS/IDR 可能已经过 socket buffer,所以头显里能看到一闪而过的画面,然后断开。

可能原因(按优先级):

1. libx264 给 2560×720@60 选了 `Constrained Baseline level 4.2`。某些 Android `MediaCodec` 实现拿到 Level 4.2 SPS 会 throw。
2. MediaDecoder 的 MediaCodec 输入队列因为 Unity `RawImage` 那边消费太慢卡住。
3. WiFi RTT 抖动(实测 47–562 ms 区间)叠加 TCP 没有 ACK 缓冲就崩。

正在试的解决方向:

- 强制 `level=31`(Baseline 3.1):`m_encCtx->level = 31` 加在 `avcodec_open2` 之前
- 把 bitrate 降到 2 Mbps,缩小 IDR 帧大小
- 缩 GOP(`gop=fps` 而不是 `2×fps`),让 SPS/PPS 更频繁出现
- 切 `h264_nvenc` 配 `delay=0`

要彻底定位,在头显里开 wireless ADB(开发者选项 → 无线调试),`adb connect <HEADSET_IP>:5555`,然后:

```bash
adb logcat -s MediaDecoder *:E
```

边推边看头显侧到底报什么错。

### 8.2 Single-accept listener

`com.picovr.robotassistantlib.MediaDecoder` 每次 Listen 只 accept 一次连接。**探端口本身就会消耗那次 accept**。不要 `nc -vz <ip> 12345` 也不要 `nmap` 头显——直接 `RobotVisionConsole --tcp-camera c` 推流就行。要重新 arm:头显里 toggle Listen 关再开。

### 8.3 `--simple-capture` 可能产生空 / 损坏的 YUV

`main.cpp:331` 调 `retrieveImage(zed_image, sl::VIEW::SIDE_BY_SIDE, sl::MEM::GPU)`(GPU 内存),但 `convertRBGAToYUV` 通过 `getPtr<sl::MEM::CPU>` 读(CPU 内存)。当数据只在 GPU 时,CPU 指针返回 null 或脏数据。如果你跑 `--simple-capture` 发现 YUV 文件全 0 或程序崩,把那一行改成 `sl::MEM::CPU` 即可。`--tcp-camera c` 走的是正确的 `sl::MEM::CPU`,没这个问题。

### 8.4 不在白名单里的 `--width × --height` 静默降级到 HD720

[3.1 节](#31-zed-分辨率分发)只承认 4 个分辨率。其他(如 `2160×1440`,头显 APK RemoteCameraWindow 的默认)会落到 HD720,stderr 打 warning,但你的编码输出仍然是 1280×720(SBS 是 2560×720)。要支持其他分辨率得改 `main.cpp` 里的分发表。

### 8.5 Q 退出只在交互终端有效

见 [3.3 节](#33-怎么停止)。非 tty 用 `Ctrl+C` / `kill` / `timeout`。

### 8.6 `--analyze-delay` 没 instrumentation 跑了等于没跑

它要的 marker 字符串当前代码不打。见 [7.2 节](#72-注意--instrumentation-还没接)。

### 8.7 ZED 偶尔进程退出后没释放 USB

`printErrorAndQuit` 直接 `std::exit(EXIT_FAILURE)`,`zed.close()` 被绕过。下次 `zed.open()` 可能报 "device busy"。等几秒,或 `pkill RobotVisionConsole` 再用 `lsusb` 检查残留进程。

### 8.8 `--tcp-camera` 文档里的 `appVideoPlayer.exe` 是 Windows-only

上游 `--tcp-camera c` 的例子用 `appVideoPlayer.exe` 当 server,Linux 没有 VideoPlayer 构建。要 PC↔PC 测试用 `--tcp-transfer s`(只是写文件不显示);头显路径才是真实使用场景。

---

## 9. 仓库结构

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

`main.cpp` — 单二进制 CLI,暴露上面这些测试函数。
`Makefile.orin` — Linux Makefile(x86_64 和 Jetson Orin 都能用)。
`CMakeLists.txt` / `build_release.bat` — Windows / VS 构建路径(见第 10 节)。

### `VideoTransferPC/VideoPlayer/`

Qt 实现的 YUV 播放器(Windows 向)。头显这条路用不上,保留只为了和上游一致。注意上游 `--tcp-camera c` 例子里用 `appVideoPlayer.exe` 当 server,Linux 没有这个构建——PC↔PC 测试用 `--tcp-transfer s`,头显路径直接推。

---

## 10. Windows 构建(legacy / 附录)

上游原本是 Windows + Visual Studio + Qt 路径,这条路也还在:

- Visual Studio 2019 Community
- Qt 6.6.x(MSVC 2019 64-bit) — 只有 `VideoPlayer` 用
- FFmpeg 7.1 — vendored(从上游的 `ffmpeg-7.1-full_build-shared/` 下载放过来)
- ASIO 1.30.2 — vendored

构建用 `VideoPlayer/build_release.bat` 和 `RobotVisionTest/build_release.bat`,产物 `RobotVisionConsole.exe` 和 `appVideoPlayer.exe`。

Linux x86_64 是现在推荐路径,见第 2 节。

---

## 11. 致谢

本项目 fork 自 [XRoboToolkit-RobotVision-PC](https://github.com/XR-Robotics/XRoboToolkit-RobotVision-PC)(GPL-3.0)。如果在学术工作里引用,请引用原始论文:

```
@article{zhao2025xrobotoolkit,
      title={XRoboToolkit: A Cross-Platform Framework for Robot Teleoperation},
      author={Zhigen Zhao and Liuchuan Yu and Ke Jing and Ning Yang},
      journal={arXiv preprint arXiv:2508.00097},
      year={2025}
}
```
