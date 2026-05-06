# RobotVisionConsole

## Build on Windows

- Update the following line in `build_release.bat`

`set VS_PATH=C:\Program Files (x86)\Microsoft Visual Studio\2019\Community\Common7\IDE\devenv.exe`

- Double click `build_release.bat` to build

- Test

```
RobotVisionConsole.exe
```

## Build on Nvidia Orin

- Build ffmpeg

```
# Build ffmpeg
git clone https://github.com/FFmpeg/nv-codec-headers.git
cd nv-codec-headers 
# tag to match the nvidia driver version (see readme)
git checkout n12.1.14.0

sudo make install

# sudo apt-get install build-essential yasm cmake libtool libc6 libc6-dev unzip wget libnum
# ./configure --enable-nonfree --enable-cuda-nvcc --enable-libnpp --extra-cflags=-I/usr/local/cuda/include --extra-ldflags=-L/usr/local/cuda/lib64 --disable-static --enableshared


sudo apt-get install build-essential yasm cmake libtool libc6 libc6-dev unzip wget

# w/ cuda // not working, reference https://forums.developer.nvidia.com/t/jetson-orin-64-dev-ffmpeg-nvenc-h264-codec-issues/310361
./configure --enable-nonfree --enable-cuda-nvcc --enable-libnpp --extra-cflags=-I/usr/local/cuda/include --extra-ldflags=-L/usr/local/cuda/lib64

# w/o cuda # Orin
./configure --enable-nonfree --enable-gpl --enable-libx264 --enable-libx265

# for nvcc
export PATH=/usr/local/cuda/bin/:$PATH

make -j 8
sudo make install
```

- Build

```
make -f Makefile.orin
```

- Test
```
./RobotVisionConsole --tcp-camera c --ip <ip address> --port <port> --fps <fps> 
```
