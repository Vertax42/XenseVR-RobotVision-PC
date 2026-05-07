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

Verify:

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

### 2.3 Verify the binary

```bash
ldd ./RobotVisionConsole | grep -E "sl_zed|cudart|libcuda\.|avcodec|opencv"
./RobotVisionConsole          # prints CLI help
```

You should see `libsl_zed.so → /usr/local/zed/lib/libsl_zed.so`, `libavcodec.so.61 → $CONDA_PREFIX/lib/...`, and `libopencv_*.so → /lib/x86_64-linux-gnu/...`.

---

## 3. Test Workflow

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

If you see `Fatal Error: Send error: Broken pipe` immediately after ZED opens, see [Section 6 — Known Issues](#6-known-issues).

---

## 4. Wire Protocol

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

Encoder configuration the upstream uses (and what MediaCodec must accept):

| Parameter | Value |
| --- | --- |
| MIME | `video/avc` (H.264) |
| Profile | Constrained Baseline |
| B-frames | 0 |
| `annexb` | 1 |
| GOP | `2 × fps` |
| Pix fmt | `yuv420p` |

---

## 5. Switching to NVENC (recommended for Blackwell / 40-series GPUs)

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

The NVENC encoder is hard-required for sustaining 2560×720 @ 60 fps on bigger SBS resolutions; libx264 ultrafast caps around 25-30 fps for a same-process encode+decode.

---

## 6. Known Issues

### `Fatal Error: Send error: Broken pipe` shortly after first sendData

The headset's MediaDecoder closes the TCP connection within the first one or two NAL units. SPS/PPS/IDR may have already crossed the TCP socket buffer, so a single frame can flash on the headset before disconnect.

Suspected causes (in priority order):

1. libx264 picks `Constrained Baseline level 4.2` for 2560×720@60. Some Android `MediaCodec` instances throw on Level 4.2 SPS.
2. MediaDecoder's MediaCodec input queue stalls because the consumer Surface (Unity `RawImage` `RawTexture`) isn't being read fast enough.
3. WiFi RTT jitter (we observed 47–562 ms range during one test) compounds with the absence of TCP ACK timing buffer.

Workarounds being investigated:

- Force `level=31` (Baseline 3.1) in the encoder.
- Reduce per-frame size by lowering bitrate to 2 Mbps.
- Smaller GOP (`gop=fps` instead of `2×fps`) so SPS/PPS reflows more often.
- Switch encoder to `h264_nvenc` with `delay=0`.

To diagnose definitively, capture `adb logcat -s MediaDecoder *:E` from the headset while streaming. Enable wireless ADB on the Pico (Developer Options → Wireless ADB), then `adb connect <HEADSET_IP>:5555`.

### Single-accept listener

`com.picovr.robotassistantlib.MediaDecoder` accepts exactly one connection per Listen click. **Probing the port consumes the accept slot.** Don't `nc -vz <ip> 12345` or `nmap` against the headset before streaming. Just go straight to `RobotVisionConsole --tcp-camera c`. To re-arm: toggle Listen off then on inside the headset APK.

---

## 7. Project Layout

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

`main.cpp` — single-binary CLI exposing the test functions below.
`Makefile.orin` — Linux Makefile (works on x86_64 and Jetson Orin).
`CMakeLists.txt` / `build_release.bat` — Windows / Visual Studio build (see Section 8).

### `VideoTransferPC/VideoPlayer/`

A Qt-based YUV player (Windows-oriented). Not required for the headset use case — kept from upstream for parity.

### `RobotVisionConsole` CLI options

| Option | Function |
| --- | --- |
| `--simple-capture` | ZED capture → write raw YUV to `captured_frames.yuv` |
| `--camera-test` | ZED → libx264 encode → in-memory decode (loopback) |
| `--tcp-transfer s` | TCP server, decodes incoming NAL stream, writes `decoded_frames.yuv` |
| `--tcp-transfer c` | TCP client, ZED LEFT view → encode → TCP send |
| `--tcp-camera c` | TCP client, ZED `SIDE_BY_SIDE` → encode → TCP send (the headset path) |
| `--analyze-delay` | Reads `Messages.dblog`, prints latency CSV |

Common parameters: `--ip <addr> --port <num> --width <px> --height <px> --fps <hz> --bitrate <bps>`. Default: `127.0.0.1:12345 1280×720@30 4Mbps`.

---

## 8. Windows Build (legacy / appendix)

The upstream codebase originally targeted Windows + Visual Studio + Qt. That path still works:

- Visual Studio 2019 Community
- Qt 6.6.x (MSVC 2019 64-bit) — needed for `VideoPlayer` only
- FFmpeg 7.1 — vendored (downloadable from upstream's `ffmpeg-7.1-full_build-shared/`)
- ASIO 1.30.2 — vendored

Build via `VideoPlayer/build_release.bat` and `RobotVisionTest/build_release.bat`. Output: `RobotVisionConsole.exe` and `appVideoPlayer.exe`.

Linux x86_64 is now the recommended path — see Section 2.

---

## 9. Acknowledgements

This project is forked from [XRoboToolkit-RobotVision-PC](https://github.com/XR-Robotics/XRoboToolkit-RobotVision-PC) (GPL-3.0). If you use the underlying methodology in academic work, please cite the original paper:

```
@article{zhao2025xrobotoolkit,
      title={XRoboToolkit: A Cross-Platform Framework for Robot Teleoperation},
      author={Zhigen Zhao and Liuchuan Yu and Ke Jing and Ning Yang},
      journal={arXiv preprint arXiv:2508.00097},
      year={2025}
}
```
