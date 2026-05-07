# XenseVR-RobotVision-PC

PC-side companion for streaming a ZED stereo camera to a Pico 4 Ultra Enterprise running the [`xense-vr-client`](https://github.com/Vertax42/xense-vr-client) APK. Captures left + right images from the ZED via Stereolabs SDK, encodes them as H.264 (libx264 by default; NVENC supported with a one-line patch), and pushes a length-prefixed Annex-B NAL stream over TCP to the headset's `MediaDecoder`, which uploads the decoded frame to a Unity texture for stereo display.

Forked from [XR-Robotics/XRoboToolkit-RobotVision-PC](https://github.com/XR-Robotics/XRoboToolkit-RobotVision-PC) (GPL-3.0).

> Chinese / 中文版: see [`README_CN.md`](README_CN.md).

---

## 1. Hardware & Software Requirements

| Component | Required |
| --- | --- |
| PC OS | Ubuntu 22.04 (Ubuntu 20 likely works, untested) on x86_64 |
| GPU | NVIDIA with NVENC (driver ≥ 525, e.g. RTX 30/40/50/PRO series) |
| Stereo camera | Any Stereolabs ZED supported by SDK 5.x (ZED Mini / ZED 2 / ZED 2i / ZED X) |
| Headset | Pico 4 Ultra Enterprise running [`xense-vr-client`](https://github.com/Vertax42/xense-vr-client) APK |
| Network | PC and headset on the same WiFi LAN, no port-blocking firewall |

PC software:

- Conda / Mamba env (e.g. `lerobot-xense`) providing:
  - `ffmpeg 7.x` (with NVENC), `libavcodec-dev`-equivalent (libavcodec / libavformat / libavutil / libswscale / libavdevice)
  - `cuda-toolkit 12.x` (`cuda-cudart-dev`, `cuda-nvcc`)
  - `gcc_linux-64` / `gxx_linux-64` (≥ 11)
- System apt packages:
  - `libopencv-dev`
  - `libasio-dev` (optional — repo also vendors `asio-1.30.2/`)
- ZED SDK ≥ 5.3 installed at `/usr/local/zed/`

System CUDA at `/usr/local/cuda/` is **not** required when building inside an activated conda env (the conda compiler wrappers inject `$CONDA_PREFIX/targets/x86_64-linux/{include,lib}` into `CPPFLAGS` / `LDFLAGS`).

### 1.1 Dependency layers

```
┌──────────────────────────────────────────────────────────┐
│  System (apt / driver)                                   │
│  ├── NVIDIA driver           ≥ 525 (provides libcuda)    │
│  ├── libopencv-dev           (system multi-arch)         │
│  ├── libasio-dev             (optional; repo vendors it) │
│  └── libavcodec-dev etc.     (optional; conda overrides) │
├──────────────────────────────────────────────────────────┤
│  conda env (e.g. lerobot-xense)                          │
│  ├── ffmpeg 7.x + full libav* + nvenc                    │
│  ├── cuda-toolkit 12.x + nvcc + cudart                   │
│  └── gcc/gxx ≥ 11 + sysroot                              │
├──────────────────────────────────────────────────────────┤
│  /usr/local/zed/   (Stereolabs SDK install)              │
│  ├── lib/libsl_zed.so   (~168 MB; dlopens libcuda.so.1)  │
│  ├── tools/{ZED_Diagnostic, ZED_Explorer, ...}           │
│  └── samples/                                            │
└──────────────────────────────────────────────────────────┘
```

Hidden dependencies that are not in the table:

- `libsl_zed.so` `dlopen`s `libcuda.so.1` from the **NVIDIA driver** (not the SDK). NVIDIA driver must be installed first.
- `libsl_zed.so` uses the CUDA driver API directly — does **not** require `/usr/local/cuda/` to exist (only the driver).

---

## 2. Quick Start (Linux + conda)

### 2.1 Install ZED SDK (one-time)

Download the matching installer from <https://www.stereolabs.com/developers/release/> and install with the conda env active:

```bash
conda activate lerobot-xense
bash ZED_SDK_Ubuntu22_cuda12.8_*_v5.3.0.zstd.run -- --skip_od_module
```

`--skip_od_module` skips the AI dependencies that get copied into `/usr/local/cuda/lib64/`. Stereo capture (the only thing this project needs) does not use them. If you later want neural depth, body tracking, or object detection, install system CUDA at `/usr/local/cuda/` and re-run the installer without `--skip_od_module`.

The installer's CUDA detection prefers `nvcc` from PATH, so the conda CUDA toolkit at `$CONDA_PREFIX/bin/nvcc` is sufficient — you do not need a system CUDA install.

#### What the installer actually does

```
bash ZED_SDK_*.run -- --skip_od_module
  ↓ makeself extract (zstd, ~3 GB) → /tmp/selfgz_xxx/
  ↓ exec ./linux_install_release.sh
    ↓ check_cuda_version()
    │   priority: command -v nvcc → /usr/local/cuda/bin/nvcc → /usr/local/cuda/version.txt
    │   matches conda env nvcc 12.8 → "OK: Found CUDA 12.8"
    ↓ check_nvidia_driver() — any nvidia-smi output passes
    ↓ Prompt for install path (default /usr/local/zed, just press Enter)
    ↓ sudo cp lib/*.so* include/* zed-config*.cmake → /usr/local/zed/
    ↓ sudo cp tools/* → /usr/local/zed/tools/                 (optional)
    ↓ AI module copy (skipped via --skip_od_module)
    ↓ apt install libjpeg-turbo8 libusb-1.0-0 libopenblas-dev mesa-utils ...
    ↓ udev rules → /lib/udev/rules.d/                          (not /etc/udev/rules.d/)
    ↓ pyzed wheel install into the active python's site-packages
    ↓ sysctl net.core.rmem_max/default = 1048576              (for Stereolabs's own streaming)
```

The installer is **idempotent** — re-running prompts to overwrite. Default install path `/usr/local/zed/` is used by `Makefile.orin`; do not change it unless you also patch the Makefile.

#### Verify

```bash
ls /usr/local/zed/lib/libsl_zed.so
python -c "import pyzed.sl as sl; print(sl.Camera.get_sdk_version())"
/usr/local/zed/tools/ZED_Diagnostic
```

### 2.2 Clone & build

```bash
git clone git@github.com:Vertax42/XenseVR-RobotVision-PC.git
cd XenseVR-RobotVision-PC/VideoTransferPC/RobotVisionTest

conda activate lerobot-xense
make -f Makefile.orin -j$(nproc)
# produces ./RobotVisionConsole
```

`Makefile.orin` is the generic Linux Makefile (its `.orin` suffix is a historical name from the upstream Jetson Orin port — it works on x86_64 too). The Makefile uses `+=` for `CPPFLAGS` / `CXXFLAGS` / `LDFLAGS` so conda's compiler-wrapper-injected paths are preserved.

#### What `make` actually does

Each `.cpp` is compiled with:

```
$(CXX) $(CPPFLAGS) $(CXXFLAGS) -c file.cpp -o file.o
```

`$(CXX) = g++` is hardcoded in the Makefile (line 9), **not** picked up from env. This is intentional: conda's compiler wrapper at `$CONDA_PREFIX/bin/x86_64-conda-linux-gnu-c++` uses a sysroot that does not search system `/usr/include`, which would prevent finding system OpenCV.

`$CPPFLAGS` actual value when conda env is active:

```
[conda env injects]
  -DNDEBUG -D_FORTIFY_SOURCE=2 -O2
  -isystem $CONDA_PREFIX/include
  -I$CONDA_PREFIX/targets/x86_64-linux/include    ← cuda.h, cuda_runtime.h
[Makefile appends]
  -std=c++17
  -I../../asio-1.30.2/include    ← vendored asio
  -I../src
  -I/usr/local/zed/include       ← Camera.hpp
  -I/usr/include/opencv4         ← system OpenCV
  -I/usr/local/cuda/include      ← nonexistent, gcc silently ignores
```

`$LDFLAGS` actual value:

```
[conda env injects]
  -Wl,-rpath,$CONDA_PREFIX/lib
  -Wl,-rpath-link,$CONDA_PREFIX/lib
  -L$CONDA_PREFIX/lib                              ← libavcodec.so
  -L$CONDA_PREFIX/targets/x86_64-linux/lib         ← libcudart.so
  -L$CONDA_PREFIX/targets/x86_64-linux/lib/stubs   ← libcuda.so stub
  + (other hardening flags)
[Makefile appends]
  -L/usr/local/zed/lib                             ← libsl_zed.so
  -L/usr/local/cuda/lib64                          ← nonexistent, harmless
  -L/usr/lib/x86_64-linux-gnu                      ← system OpenCV
  + -lavcodec -lavformat -lavutil -lswscale -lavdevice -lsl_zed -lcuda -lcudart -lopencv_*  -lpthread
```

#### Runtime library resolution

After build, `ldd ./RobotVisionConsole` should show:

```
libavcodec.so.61   → $CONDA_PREFIX/lib/libavcodec.so.61   (rpath)
libsl_zed.so       → /usr/local/zed/lib/libsl_zed.so       (-L)
libopencv_*.so     → /lib/x86_64-linux-gnu/libopencv_*.so  (-L)
libcuda.so.1       → /lib/x86_64-linux-gnu/libcuda.so.1    (NVIDIA driver provides)
libcudart.so.12    → $CONDA_PREFIX/.../libcudart.so.12     (rpath)
```

Conda env activation is **not strictly required at runtime** — rpath bakes the conda lib paths into the binary. As long as the env still exists at the same path, the binary runs. **Conda env activation IS required at build time** (otherwise `$CPPFLAGS` is not injected and the build fails to find `cuda.h`).

### 2.3 Verify the binary

```bash
ldd ./RobotVisionConsole | grep -E "sl_zed|cudart|libcuda\.|avcodec|opencv"
./RobotVisionConsole          # prints CLI help
```

---

## 3. CLI Reference

`RobotVisionConsole` exposes 6 modes as subcommands. All share these options (defaults match the upstream's Windows DirectShow assumptions):

| Option | Default | Notes |
| --- | --- | --- |
| `--ip <addr>` | `127.0.0.1` | TCP target IP for client / bind IP for server |
| `--port <num>` | `12345` | TCP port |
| `--width <px>` | `1280` | Encoder input width (single eye; for `--tcp-camera c` it is doubled internally) |
| `--height <px>` | `720` | Encoder input height |
| `--fps <hz>` | `30` | Capture / encode framerate |
| `--bitrate <bps>` | `4000000` | Encoder bitrate. Only used by `--tcp-camera c` |
| `--camera <str>` | `video=Integrated Webcam` | **Ignored on Linux** (legacy from Windows DirectShow) |

### 3.1 ZED resolution dispatch

`--width × --height` is matched against a fixed list of ZED native modes (`RobotVisionTest/main.cpp:158-179` and copies in other test functions):

| `--width × --height` | ZED mode | Single-eye | SBS (= 2W × H) |
| --- | --- | --- | --- |
| `1920 × 1080` | `HD1080` | 1920×1080 | 3840×1080 |
| `1280 × 720` | `HD720` | 1280×720 | 2560×720 |
| `2208 × 1242` | `HD2K` | 2208×1242 | 4416×1242 |
| `672 × 376` | `VGA` | 672×376 | 1344×376 |
| anything else | falls back to `HD720` + warning | 1280×720 | 2560×720 |

Custom resolutions (e.g. `2160×1440`) are **silently downgraded to HD720**. Edit the dispatch in `main.cpp` if you need additional modes.

### 3.2 Modes

| Mode | Function | ZED view | Encoder input | Network role | Output |
| --- | --- | --- | --- | --- | --- |
| `--simple-capture` | `RunSimpleCameraCaptureTest` | `SIDE_BY_SIDE` | n/a (raw save) | none | `captured_frames.yuv` |
| `--camera-test` | `RunCameraCaptureTest` | `LEFT` | `W × H` | self-loopback (in-process) | stdout |
| `--tcp-transfer s` | `runH264TCPTransferTest("s")` | — (no camera) | — | TCP server | `decoded_frames.yuv` |
| `--tcp-transfer c` | `runH264TCPTransferTest("c")` | `LEFT` | `W × H` | TCP client | network out |
| `--tcp-camera c` | `runH264TCPCameraCaptureTest` | `SIDE_BY_SIDE` | `2W × H` ⚠️ | TCP client | network out |
| `--analyze-delay` | `analyzeDelay` | — | — | none | `output.csv` from `Messages.dblog` |

⚠️ Only `--tcp-camera c` doubles width inside the H264Encoder constructor (`H264Encoder(W*2, H, ...)`). Other modes that retrieve `SIDE_BY_SIDE` (`--simple-capture`) write the literal 2W frame to file as-is.

### 3.3 Stop / abort

The interactive "Press Q to quit" code uses `_kbhit() / _getch()` (Linux mock with termios + `O_NONBLOCK`). Q **only works in an interactive terminal**. In any of these situations Q is ignored:

- output redirected to a file or pipe
- program in background (`nohup`, `screen`/`tmux` may not preserve a tty)
- stdin closed / `</dev/null`

For non-interactive use, stop with `Ctrl+C` (SIGINT), `kill <pid>`, or wrap with `timeout N command`. The code has **no SIGINT/SIGTERM handler**, so `zed.close()` is bypassed on signal — wait a few seconds before re-opening ZED to let the kernel release the USB device.

### 3.4 Sender vs receiver internals

**Sender** (`CameraDataSender`, used by `--tcp-transfer c` and `--tcp-camera c`):

```c++
sender.startConnect(run);   // async_connect — does NOT block on success/failure
sender.runIOContext();      // io_context.run() — runs until socket closes
```

`startConnect` returns immediately; the `run` lambda (which opens ZED, encodes, sends) fires from the asio callback **after connect completes**. A failed connect can still print "ZED Camera successfully opened" before the broken pipe error — the lambda was triggered regardless.

`sendData` uses synchronous blocking `m_socket.send(...)`. Failure throws `SendDataException`, caught by `printErrorAndQuit` which calls `std::exit(EXIT_FAILURE)` — the entire process dies.

**Receiver** (`CameraDataReceiver`, used by `--tcp-transfer s`):

`acceptConnections()` recursively re-accepts after a client disconnects (`CameraDataReceiver.cpp:64`):

```c++
if (!m_should_exit) {
    acceptConnections();   // accept the next client
}
```

So `--tcp-transfer s` is **multi-shot** — keep reconnecting clients without restarting the server. **This differs from the headset side**: `com.picovr.robotassistantlib.MediaDecoder` is **single-accept**: one Listen click → one `ServerSocket.accept()` → one streaming session → `ServerSocket.close()` at end.

---

## 4. Test Workflow

Walk through phases in order. Don't skip — each phase isolates the next layer of failure mode.

### Phase 1 — ZED hardware sanity (pyzed only)

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

Expected: model + serial print, then 5 frames at `2560x720` (= 2× HD720 SBS).

If this fails: USB plug not in a SuperSpeed (USB 3) port, or you're not in the `plugdev` group, or ZED SDK install was incomplete.

### Phase 2 — PC self-loopback (ZED → encode → decode in one process)

```bash
cd VideoTransferPC/RobotVisionTest
./RobotVisionConsole --camera-test --width 1280 --height 720 --fps 60
# Press Q to stop. Keeps printing "Decoded frame size: 1382400 bytes, resolution: 1280x720"
# (1382400 = 1280*720*1.5 = YUV420 planes for the LEFT eye)
```

Validates: ZED capture, libx264 encode, in-memory decode, callback chain.

### Phase 3 — TCP self-loopback (server + client on same machine)

```bash
# Terminal A — server (decodes incoming NALUs, dumps YUV to disk)
./RobotVisionConsole --tcp-transfer s --ip 127.0.0.1 --port 12345

# Terminal B — client (ZED → encode → TCP send to 127.0.0.1)
./RobotVisionConsole --tcp-transfer c --ip 127.0.0.1 --port 12345 \
    --width 1280 --height 720 --fps 60
```

After a few seconds the server writes `decoded_frames.yuv` of size `N × 1382400`. The wire format here is byte-identical to the one the headset's `MediaDecoder` consumes (4-byte big-endian length prefix + Annex-B NAL).

### Phase 4 — Push to the Pico headset (real test)

#### Headset side
1. Connect the Pico 4 Ultra to the same WiFi LAN as the PC. Note the headset IP.
2. Launch `xense-vr-client` APK on the headset.
3. Open the Camera Ctrl panel, toggle **Listen** on.
4. When prompted, enter the **PC's IP** (the address the PC has on that LAN).

The APK opens an `MediaDecoder` TCP listener on **port 12345** (hardcoded at `Assets/Scripts/UI/UICameraCtrl.cs:39` of [`xense-vr-client`](https://github.com/Vertax42/xense-vr-client)).

> **The listener is single-accept**: one connection, one session. If the connection drops, you must toggle Listen off and on again to re-arm. Do not "probe" the port with `nc` / `nmap` — the probe itself consumes the accept slot.

#### PC side

```bash
./RobotVisionConsole --tcp-camera c \
    --ip <HEADSET_IP> --port 12345 \
    --width 1280 --height 720 \
    --fps 60 --bitrate 4000000
```

This uses ZED `sl::VIEW::SIDE_BY_SIDE` so the encoder receives a `2560×720` frame (HD720 doubled in width). The headset's `MediaCodec` decodes and uploads the texture to a Unity `RawImage`; the user sees stereo-3D in the headset.

If you see `Fatal Error: Send error: Broken pipe` immediately after ZED opens, see [Section 8 — Known Issues](#8-known-issues).

### 4.1 Common command scenarios

#### First-time deploy on a new machine

```bash
git clone git@github.com:Vertax42/XenseVR-RobotVision-PC.git ~/XenseVR-RobotVision-PC
cd ~/XenseVR-RobotVision-PC
bash ZED_SDK_*.run -- --skip_od_module        # pre-downloaded
conda activate lerobot-xense
cd VideoTransferPC/RobotVisionTest
make -f Makefile.orin -j$(nproc)              # produces ./RobotVisionConsole
```

#### Rebuild after editing source

```bash
conda activate lerobot-xense
cd ~/XenseVR-RobotVision-PC/VideoTransferPC/RobotVisionTest
make -f Makefile.orin -j$(nproc)              # incremental
# or
make -f Makefile.orin clean && make -f Makefile.orin -j$(nproc)   # full
```

#### Record a ZED stream to a local file for offline playback

```bash
./RobotVisionConsole --simple-capture --width 1280 --height 720 --fps 60
# Generates captured_frames.yuv (SBS YUV420, 2560 × 720)
# Replay with ffplay:
ffplay -f rawvideo -pixel_format yuv420p -video_size 2560x720 -framerate 60 captured_frames.yuv
```

(See known issue 8.3 about `--simple-capture` GPU/CPU memory mismatch.)

#### Cleanup

```bash
make -f Makefile.orin clean    # remove .o and binary
rm -f captured_frames.yuv decoded_frames.yuv frame.jpg Messages.dblog output.csv
```

---

## 5. Wire Protocol

Each H.264 NAL is sent over TCP framed as:

```
| 4-byte big-endian length (uint32 N) | N bytes of Annex-B NAL data |
```

The Annex-B start code `00 00 00 01` is included in the payload (libx264 with `annexb=1` produces this).

Headset side (`com.picovr.robotassistantlib.MediaDecoder` from the `xense-vr-client` AAR):

1. `ServerSocket.accept()` once
2. `DataInputStream.readFully(4)` → `uint32` length (BIG_ENDIAN)
3. `DataInputStream.readFully(length)` → NAL payload
4. Feed payload to Android `MediaCodec` (mime `video/avc`)
5. Loop step 2-4 until any I/O error → close socket → thread exits → `ServerSocket.close()`

There is no protocol-level flow control. If the headset's MediaCodec input queue blocks, our `sendData` will eventually broken-pipe.

### 5.1 Byte-level handshake (one full session)

```
TCP handshake
  PC → Pico: SYN
  Pico → PC: SYN-ACK   (Pico's ServerSocket.accept() returns the Socket)
  PC → Pico: ACK

[NALU 1 — typically combined SPS + PPS + SEI, ~200-300 B]
  PC writes:  4-byte BE length=0x000000C8 + 0x00 0x00 0x00 0x01 0x67 (SPS) ...
                                            0x00 0x00 0x00 0x01 0x68 (PPS) ...
                                            0x00 0x00 0x00 0x01 0x06 (SEI) ...
  Pico reads: readFully(4) → length=200, readFully(200) → payload
  Pico decode(): MediaCodec.configure(csd-0=SPS, csd-1=PPS); no output frame yet

[NALU 2 — IDR keyframe, ~70 KB at 4 Mbps × GOP=120]
  PC writes:  length=0x000114A4 + 00 00 00 01 65 (IDR) + slice bytes ...
  Pico reads: readFully(4)=70820, readFully(70820)
  Pico decode(): MediaCodec decodes → outputs frame to Surface → Unity texture update

[Subsequent P-frames, ~5-10 KB each]
  ... until broken pipe / Q / next GOP boundary (every 120 frames another IDR)
```

### 5.2 Encoder configuration (`H264Encoder.cpp:67-94`)

| Parameter | Value | Notes |
| --- | --- | --- |
| MIME | `video/avc` (H.264) | `AV_CODEC_ID_H264` |
| Profile | Constrained Baseline | no CABAC, no B-frames |
| `max_b_frames` | `0` | no reorder needed at decoder |
| `annexb` | `1` | start codes `00 00 00 01` instead of length prefix in the bytestream |
| `gop_size` | `2 × fps` | one IDR every 2 seconds |
| `pix_fmt` | `yuv420p` | |
| `preset` | `ultrafast` | libx264 |
| `tune` | `zerolatency` | libx264 |
| `sc_threshold` | `0` | disable scene-change detection — GOP fully determined by `gop_size` |

`annexb=1` makes libx264 emit `00 00 00 01` start codes. **Required** for MediaCodec compatibility — the headset's `MediaDecoder` parses csd-0 (SPS) and csd-1 (PPS) by start-code location. If you change this to `0`, MediaCodec configuration on the headset fails silently.

---

## 6. Switching to NVENC

`H264Encoder.cpp:27` calls `avcodec_find_encoder(AV_CODEC_ID_H264)` which selects libx264 by default. For NVENC:

```cpp
codec = avcodec_find_encoder_by_name("h264_nvenc");
if (!codec) {
    codec = avcodec_find_encoder(AV_CODEC_ID_H264);  // libx264 fallback
}
```

And replace the libx264 `av_opt_set` block with NVENC presets:

```cpp
av_opt_set     (m_encCtx->priv_data, "preset",      "p1",      0);  // p1 = fastest
av_opt_set     (m_encCtx->priv_data, "tune",        "ull",     0);  // ultra-low-latency
av_opt_set     (m_encCtx->priv_data, "rc",          "cbr",     0);
av_opt_set     (m_encCtx->priv_data, "zerolatency", "1",       0);
av_opt_set     (m_encCtx->priv_data, "profile",     "baseline",0);
av_opt_set_int (m_encCtx->priv_data, "delay",       0,         0);
```

NVENC is effectively required for sustaining higher SBS resolutions (≥ 2560×720@60); libx264 ultrafast caps around 25-30 fps for a same-process encode+decode at 2560×720.

---

## 7. analyze-delay (offline timing tool)

`--analyze-delay` is **not** an instrumented profiler — it is an offline log analyzer that needs a `Messages.dblog` file produced by application code with specific marker strings.

### 7.1 Expected log format

```
<seq:int> <timestamp:float seconds> <pid:int> <procName:string> <message...>
```

Example:

```
1 1736400000.123456 12345 RobotVisionConsole get frame from camera
2 1736400000.124000 12345 RobotVisionConsole get yuv420 frame
3 1736400000.125000 12345 RobotVisionConsole encode done send size 8123
4 1736400000.130000 12345 RobotVisionConsole dataCallback decode input size 8123
5 1736400000.131000 12345 RobotVisionConsole frameCallback decode output size 1382400
6 1736400000.132000 12345 RobotVisionConsole presentFrame done
```

Six stage markers — the line is recognized only if the message **contains** the substring exactly:

| Stage | Marker | Meaning |
| --- | --- | --- |
| 0 | `get frame from camera` | ZED `grab()` returned |
| 1 | `get yuv420 frame` | RGBA → YUV conversion done |
| 2 | `encode done send size` | libx264 finished one frame |
| 3 | `dataCallback decode input size` | Receiver got a complete NALU |
| 4 | `frameCallback decode output size` | Decoder emitted a YUV frame |
| 5 | `presentFrame done` | Display done |

Output `output.csv`:

```
Capture,Encode,tcp transfer,Decode,Present
0.544,1.000,5.000,1.000,1.000
...
```

Each row is one cycle's per-stage delta in milliseconds. The last row contains the per-column averages.

### 7.2 Caveat — instrumentation is missing

**The current code does not emit any of these marker strings.** To make `--analyze-delay` useful, you must add instrumentation logging to your camera capture loop, encoder, receiver, and presenter (and have them all log to `Messages.dblog` with the format above). This is an **unfinished framework** — the analyzer is built but the producers are not wired up.

---

## 8. Known Issues

### 8.1 `Fatal Error: Send error: Broken pipe` shortly after first sendData

The headset's MediaDecoder closes the TCP connection within the first one or two NAL units. SPS/PPS/IDR may have already crossed the TCP socket buffer, so a single frame can flash on the headset before disconnect.

Suspected causes (in priority order):

1. libx264 picks `Constrained Baseline level 4.2` for 2560×720@60. Some Android `MediaCodec` instances throw on Level 4.2 SPS.
2. MediaDecoder's MediaCodec input queue stalls because the consumer Surface (Unity `RawImage` `RawTexture`) isn't being read fast enough.
3. WiFi RTT jitter (we observed 47–562 ms range during one test) compounds with the absence of TCP ACK timing buffer.

Workarounds being investigated:

- Force `level=31` (Baseline 3.1) in the encoder (`m_encCtx->level = 31` before `avcodec_open2`).
- Reduce per-frame size by lowering bitrate to 2 Mbps.
- Smaller GOP (`gop=fps` instead of `2×fps`) so SPS/PPS reflows more often.
- Switch encoder to `h264_nvenc` with `delay=0`.

To diagnose definitively, capture `adb logcat -s MediaDecoder *:E` from the headset while streaming. Enable wireless ADB on the Pico (Developer Options → Wireless ADB), then `adb connect <HEADSET_IP>:5555`.

### 8.2 Single-accept listener

`com.picovr.robotassistantlib.MediaDecoder` accepts exactly one connection per Listen click. **Probing the port consumes the accept slot.** Don't `nc -vz <ip> 12345` or `nmap` against the headset before streaming. Just go straight to `RobotVisionConsole --tcp-camera c`. To re-arm: toggle Listen off then on inside the headset APK.

### 8.3 `--simple-capture` may produce empty / corrupt YUV

`main.cpp:331` calls `retrieveImage(zed_image, sl::VIEW::SIDE_BY_SIDE, sl::MEM::GPU)` (GPU memory) but `convertRBGAToYUV` reads via `getPtr<sl::MEM::CPU>` (CPU memory). When the data only lives in GPU, the CPU pointer can return null/garbage. If your `captured_frames.yuv` is all zeros or the program crashes, fix by changing the line to `sl::MEM::CPU`. The `--tcp-camera c` path uses `sl::MEM::CPU` correctly and is unaffected.

### 8.4 Unsupported `--width × --height` silently downgrades to HD720

Only the four resolutions in [Section 3.1](#31-zed-resolution-dispatch) are honored. Anything else (e.g. `2160×1440`, the headset APK's RemoteCameraWindow default) falls back to HD720 with a warning printed to stderr — your encoded output is still 1280×720 (or 2560×720 SBS). To add custom resolutions, edit the dispatch in `main.cpp`.

### 8.5 Q-to-stop only works in interactive terminals

See [Section 3.3](#33-stop--abort). For non-tty contexts, use `Ctrl+C`, `kill`, or `timeout`.

### 8.6 `--analyze-delay` does nothing without instrumentation

The marker strings it looks for are not emitted by the current code. See [Section 7.2](#72-caveat--instrumentation-is-missing).

### 8.7 ZED occasionally fails to release USB after process exit

Because `printErrorAndQuit` calls `std::exit(EXIT_FAILURE)` directly, `zed.close()` is bypassed. The next `zed.open()` may fail with "device busy" for a few seconds. Wait, or `pkill RobotVisionConsole` and recheck `lsusb` before retrying.

### 8.8 `appVideoPlayer.exe` reference in `--tcp-camera` doc is Windows-only

Upstream's example for `--tcp-camera c` uses `appVideoPlayer.exe` as the TCP server. There is no Linux build of `VideoPlayer` here. For a PC↔PC test use `--tcp-transfer s` (which is similar but writes to file instead of displaying); the headset path is the production use.

---

## 9. Project Layout

### `VideoTransferPC/src/`

Core library (compiled into `RobotVisionConsole`):

| File | Role |
| --- | --- |
| `CameraCapture.cpp/h` | Generic camera capture (DirectShow on Windows; ZED SDK is invoked directly from `main.cpp` on Linux) |
| `CameraDataSender.cpp/h` | TCP client wrapping asio, with the 4-byte length prefix protocol |
| `CameraDataReceiver.cpp/h` | TCP server counterpart |
| `H264Encoder.cpp/h` | FFmpeg encoder wrapper (libx264 by default) |
| `H264Decoder.cpp/h` | FFmpeg decoder wrapper |
| `H264NALUParser.cpp/h` | Annex-B NAL parsing helpers |
| `NetworkVideoSource.cpp/h` | High-level video source pulling encoded frames from network |
| `FFmpegUtils.cpp/h` | Misc FFmpeg helpers |
| `SolidColorFrame.cpp/h`, `ColorFrameGenerator.cpp/h` | Synthetic test patterns |

### `VideoTransferPC/RobotVisionTest/`

`main.cpp` — single-binary CLI exposing the test functions above.
`Makefile.orin` — Linux Makefile (works on x86_64 and Jetson Orin).
`CMakeLists.txt` / `build_release.bat` — Windows / Visual Studio build (see Section 10).

### `VideoTransferPC/VideoPlayer/`

A Qt-based YUV player (Windows-oriented). Not required for the headset use case — kept from upstream for parity. Note that the upstream README example for `--tcp-camera c` uses `appVideoPlayer.exe` as the server side; on Linux there is no `appVideoPlayer` build, so use `--tcp-transfer s` for self-loopback or stream directly to the headset.

---

## 10. Windows Build (legacy / appendix)

The upstream codebase originally targeted Windows + Visual Studio + Qt. That path still works:

- Visual Studio 2019 Community
- Qt 6.6.x (MSVC 2019 64-bit) — needed for `VideoPlayer` only
- FFmpeg 7.1 — vendored (downloadable from upstream's `ffmpeg-7.1-full_build-shared/`)
- ASIO 1.30.2 — vendored

Build via `VideoPlayer/build_release.bat` and `RobotVisionTest/build_release.bat`. Output: `RobotVisionConsole.exe` and `appVideoPlayer.exe`.

Linux x86_64 is now the recommended path — see Section 2.

---

## 11. Acknowledgements

This project is forked from [XRoboToolkit-RobotVision-PC](https://github.com/XR-Robotics/XRoboToolkit-RobotVision-PC) (GPL-3.0). If you use the underlying methodology in academic work, please cite the original paper:

```
@article{zhao2025xrobotoolkit,
      title={XRoboToolkit: A Cross-Platform Framework for Robot Teleoperation},
      author={Zhigen Zhao and Liuchuan Yu and Ke Jing and Ning Yang},
      journal={arXiv preprint arXiv:2508.00097},
      year={2025}
}
```
