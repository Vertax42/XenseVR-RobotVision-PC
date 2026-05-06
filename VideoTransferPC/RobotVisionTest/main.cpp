class SendDataException;

//Console application for testing FFmpeg camera capture and video streaming functionality
extern "C" {
#include <libavcodec/avcodec.h>
#include <libavformat/avformat.h>
#include <libavdevice/avdevice.h>
#include <libswscale/swscale.h>
#include <libavutil/imgutils.h>
}

// ZED includes
#include <sl/Camera.hpp>

// OpenCV include (for display)
#include <opencv2/opencv.hpp>

#include <stdio.h>
#include <inttypes.h>  // For format specifiers like PRIu64
#include <vector>
#include <string>
#include <functional>
#include <algorithm>
#include <stdexcept>  // Add this line to support std::runtime_error
#include "H264Encoder.h"
#include "H264Decoder.h"
#include "FFmpegUtils.h"
#include "H264NALUParser.h"
#include <asio.hpp>
#include <iostream>
#include <thread>
#include <chrono>

#ifdef _WIN32
#include <conio.h> // Add this line to use _kbhit() and _getch()
#include <windows.h> // For OutputDebugStringA
#else
#include <termios.h> // For _kbhit and _getch on Linux
#include <unistd.h>
#include <fcntl.h>

// Mock OutputDebugStringA for Linux
#define OutputDebugStringA(x) std::cerr << x
// Mock _kbhit and _getch for Linux
int _kbhit() {
    struct termios oldt, newt;
    int ch;
    int oldf;

    tcgetattr(STDIN_FILENO, &oldt);
    newt = oldt;
    newt.c_lflag &= ~(ICANON | ECHO);
    tcsetattr(STDIN_FILENO, TCSANOW, &newt);
    oldf = fcntl(STDIN_FILENO, F_GETFL, 0);
    fcntl(STDIN_FILENO, F_SETFL, oldf | O_NONBLOCK);

    ch = getchar();

    tcsetattr(STDIN_FILENO, TCSANOW, &oldt);
    fcntl(STDIN_FILENO, F_SETFL, oldf);

    if (ch != EOF) {
        ungetc(ch, stdin);
        return 1;
    }

    return 0;
}

int _getch() {
    int ch;
    struct termios oldt, newt;
    tcgetattr(STDIN_FILENO, &oldt);
    newt = oldt;
    newt.c_lflag &= ~(ICANON | ECHO);
    tcsetattr(STDIN_FILENO, TCSANOW, &newt);
    ch = getchar();
    tcsetattr(STDIN_FILENO, TCSANOW, &oldt);
    return ch;
}

#endif

#include "CameraDataReceiver.h"
#include "CameraDataSender.h"
#include "NetworkVideoSource.h"
#include <fstream>
#include <sstream>
#include <iomanip>

// Function to convert RGBA (from sl::Mat) to YUV (I420/YUV420p)
void convertRBGAToYUV(const sl::Mat & rgba_frame, std::vector<uint8_t>&y_plane,
    std::vector<uint8_t>&u_plane, std::vector<uint8_t>&v_plane) {
    int width = rgba_frame.getWidth();
    int height = rgba_frame.getHeight();
    size_t rgba_step = rgba_frame.getStepBytes(); // Bytes per row for RGBA

    // Resize Y, U, V planes
    y_plane.resize(width * height);
    u_plane.resize((width / 2) * (height / 2));
    v_plane.resize((width / 2) * (height / 2));

    const unsigned char* rgba_data = rgba_frame.getPtr<unsigned char>(sl::MEM::CPU);

    // Populate Y plane
    for (int i = 0; i < height; ++i) {
        for (int j = 0; j < width; ++j) {
            size_t rgba_idx = i * rgba_step + j * 4; // 4 bytes per pixel (R, G, B, A)
            unsigned char B = rgba_data[rgba_idx];
            unsigned char G = rgba_data[rgba_idx + 1];
            unsigned char R = rgba_data[rgba_idx + 2];

            // Y = 0.299R + 0.587G + 0.114B
            y_plane[i * width + j] = static_cast<uint8_t>(0.299 * R + 0.587 * G + 0.114 * B);
        }
    }

    // Populate U and V planes (4:2:0 subsampling)
    // For simplicity, sample U and V from the top-left pixel of each 2x2 block.
    for (int i = 0; i < height; i += 2) {
        for (int j = 0; j < width; j += 2) {
            size_t rgba_idx = i * rgba_step + j * 4;
            unsigned char B = rgba_data[rgba_idx];
            unsigned char G = rgba_data[rgba_idx + 1];
            unsigned char R = rgba_data[rgba_idx + 2];

            // U = -0.147R - 0.289G + 0.436B + 128
            u_plane[(i / 2) * (width / 2) + (j / 2)] = static_cast<uint8_t>(-0.147 * R - 0.289 * G + 0.436 * B + 128);

            // V = 0.615R - 0.515G - 0.100B + 128
            v_plane[(i / 2) * (width / 2) + (j / 2)] = static_cast<uint8_t>(0.615 * R - 0.515 * G - 0.100 * B + 128);
        }
    }
}


// main function
int RunCameraCaptureTest(char* captureDevice, int resolution_width, int resolution_height, int frameRate) {
    // Global initialization (only call once)
    avdevice_register_all();

    // Define decoder callback function
    auto decode_callback = [](const uint8_t* data, size_t size, int width, int height) {
        printf("Decoded frame size: %zu bytes, resolution: %dx%d\n", size, width, height);
    };

    // Create H264Decoder instance
    H264Decoder h264_decoder(decode_callback);

    // Define encoder callback function
    auto encoder_callback = [&h264_decoder](const uint8_t* data, size_t size) {
        h264_decoder.decode(data, size);
    };

    // Create encoder instance
    H264Encoder h264_encoder(resolution_width, resolution_height, encoder_callback, frameRate);

    // ZED Camera setup
    sl::Camera zed;
    sl::InitParameters init_parameters;

    // Set desired ZED resolution. Choose the closest or compatible one.
    if (resolution_width == 1920 && resolution_height == 1080) {
        init_parameters.camera_resolution = sl::RESOLUTION::HD1080;
    }
    else if (resolution_width == 1280 && resolution_height == 720) {
        init_parameters.camera_resolution = sl::RESOLUTION::HD720;
    }
    else if (resolution_width == 2208 && resolution_height == 1242) {
        init_parameters.camera_resolution = sl::RESOLUTION::HD2K;
    }
    else if (resolution_width == 672 && resolution_height == 376) {
        init_parameters.camera_resolution = sl::RESOLUTION::VGA;
    }
    else {
        std::cerr << "Warning: Requested resolution " << resolution_width << "x" << resolution_height
            << " not directly supported by ZED SDK. Using HD720 as default." << std::endl;
        init_parameters.camera_resolution = sl::RESOLUTION::HD720;
    }
    init_parameters.camera_fps = frameRate;
    init_parameters.depth_mode = sl::DEPTH_MODE::NONE; // We only need RGB here

    // Open the camera
    sl::ERROR_CODE err = zed.open(init_parameters);
    if (err != sl::ERROR_CODE::SUCCESS) {
        std::cerr << "Error " << err << " opening ZED Camera" << std::endl;
        return -1;
    }

    // Verify ZED camera resolution against requested encoder resolution
    if (zed.getCameraInformation().camera_configuration.resolution.width != resolution_width ||
        zed.getCameraInformation().camera_configuration.resolution.height != resolution_height) {
        std::cerr << "Warning: ZED camera resolution ("
            << zed.getCameraInformation().camera_configuration.resolution.width << "x"
            << zed.getCameraInformation().camera_configuration.resolution.height
            << ") does not exactly match requested encoder resolution ("
            << resolution_width << "x" << resolution_height << "). "
            << "Image might be scaled or distorted by encoder if it doesn't handle mismatches." << std::endl;
    }

    bool continue_capture = true;
    int current_frame_idx = 0;

    std::thread input_thread([&continue_capture]() {
        std::cout << "Press Q to stop capturing..." << std::endl;
        while (true) {
            if (_kbhit()) {
                char ch = _getch();
                if (ch == 'q' || ch == 'Q') {
                    continue_capture = false;
                    break;
                }
            }
            std::this_thread::sleep_for(std::chrono::milliseconds(100));
        }
        });

    sl::Mat zed_image; // ZED SDK's image format

    while (continue_capture) {
        if (zed.grab() == sl::ERROR_CODE::SUCCESS) {
            // Retrieve image in RGBA format
            zed.retrieveImage(zed_image, sl::VIEW::LEFT, sl::MEM::CPU);

            // Convert sl::Mat (RGBA) to YUV (I420/YUV420p)
            std::vector<uint8_t> y_plane, u_plane, v_plane;
            convertRBGAToYUV(zed_image, y_plane, u_plane, v_plane);

            // Pass YUV planes to the encoder
            printf("Processing frame %d\n", current_frame_idx++);
            h264_encoder.encodeFrame(y_plane.data(), u_plane.data(), v_plane.data(),
                y_plane.size(), u_plane.size(), v_plane.size());

            // Convert sl::Mat to cv::Mat (share buffer)
            cv::Mat cvImage = cv::Mat((int)zed_image.getHeight(), (int)zed_image.getWidth(), CV_8UC4, zed_image.getPtr<sl::uchar1>(sl::MEM::CPU));


            //Display the image
            cv::imwrite("frame.jpg", cvImage);
        }
        else {
            std::this_thread::sleep_for(std::chrono::milliseconds(1)); // Avoid busy-waiting
        }
    }

    zed.close(); // Close the ZED camera
    input_thread.join();

    printf("Capture completed.\n");

    return 0;
}

int RunSimpleCameraCaptureTest(char* captureDevice, int resolution_width, int resolution_height, int frameRate) {
    // Global initialization (only call once)
    avdevice_register_all();

    // Create output file
    FILE* output_file = fopen("captured_frames.yuv", "wb");
    if (!output_file) {
        fprintf(stderr, "Failed to create output file captured_frames.yuv\n");
        return -1;
    }

    // ZED Camera setup
    sl::Camera zed;
    sl::InitParameters init_parameters;
    // Set desired ZED resolution. Choose the closest or compatible one.
    if (resolution_width == 1920 && resolution_height == 1080) {
        init_parameters.camera_resolution = sl::RESOLUTION::HD1080;
    }
    else if (resolution_width == 1280 && resolution_height == 720) {
        init_parameters.camera_resolution = sl::RESOLUTION::HD720;
    }
    else if (resolution_width == 2208 && resolution_height == 1242) {
        init_parameters.camera_resolution = sl::RESOLUTION::HD2K;
    }
    else if (resolution_width == 672 && resolution_height == 376) {
        init_parameters.camera_resolution = sl::RESOLUTION::VGA;
    }
    else {
        std::cerr << "Warning: Requested resolution " << resolution_width << "x" << resolution_height
            << " not directly supported by ZED SDK. Using HD720 as default." << std::endl;
        init_parameters.camera_resolution = sl::RESOLUTION::HD720;
    }
    init_parameters.camera_fps = frameRate;
    init_parameters.depth_mode = sl::DEPTH_MODE::NONE; // We only need RGB here

    // Open the camera
    sl::ERROR_CODE err = zed.open(init_parameters);
    if (err != sl::ERROR_CODE::SUCCESS) {
        std::cerr << "Error " << err << " opening ZED Camera" << std::endl;
        fclose(output_file);
        return -1;
    }

    // Verify ZED camera resolution against requested encoder resolution
    if (zed.getCameraInformation().camera_configuration.resolution.width != resolution_width ||
        zed.getCameraInformation().camera_configuration.resolution.height != resolution_height) {
        std::cerr << "Warning: ZED camera resolution ("
            << zed.getCameraInformation().camera_configuration.resolution.width << "x"
            << zed.getCameraInformation().camera_configuration.resolution.height
            << ") does not exactly match requested encoder resolution ("
            << resolution_width << "x" << resolution_height << "). "
            << "Frames might be scaled or distorted upon saving if encoder doesn't handle mismatches." << std::endl;
    }


    bool continue_capture = true;
    int current_frame_idx = 0;

    std::thread input_thread([&continue_capture]() {
        std::cout << "Press Q to stop capturing..." << std::endl;
        while (true) {
            if (_kbhit()) {
                char ch = _getch();
                if (ch == 'q' || ch == 'Q') {
                    continue_capture = false;
                    break;
                }
            }
            std::this_thread::sleep_for(std::chrono::milliseconds(100));
        }
        });

    sl::Mat zed_image; // ZED SDK's image format

    while (continue_capture) {
        if (zed.grab() == sl::ERROR_CODE::SUCCESS) {
            // Retrieve image in RGBA format
            zed.retrieveImage(zed_image, sl::VIEW::SIDE_BY_SIDE, sl::MEM::GPU);

            // Convert sl::Mat (RGBA) to YUV (I420/YUV420p)
            std::vector<uint8_t> y_plane, u_plane, v_plane;
            convertRBGAToYUV(zed_image, y_plane, u_plane, v_plane);

            // Write current frame Y, U, V planes to file
            fwrite(y_plane.data(), 1, y_plane.size(), output_file);
            fwrite(u_plane.data(), 1, u_plane.size(), output_file);
            fwrite(v_plane.data(), 1, v_plane.size(), output_file);
            printf("Wrote frame %d to captured_frames.yuv\n", ++current_frame_idx);
        }
        else {
            std::this_thread::sleep_for(std::chrono::milliseconds(1)); // Avoid busy-waiting
        }
    }

    zed.close(); // Close the ZED camera
    input_thread.join();
    fclose(output_file); // Close file
    printf("All frames have been written to captured_frames.yuv\n");
    printf("Capture completed.\n");

    return 0;
}


int runH264TCPTransferTest(int argc, char* argv[], const std::string & ip, int port, int resolution_width, int resolution_height, int frameRate, const std::string & camera_name) {
    if (argc != 2) {
        // This usage message will be handled by the main function's printUsage
        return 1;
    }

    std::string mode = argv[1];
    if (mode == "s") {
        // Modify to use std::ofstream to open file
        std::ofstream output_file("decoded_frames.yuv", std::ios::binary);

        auto videoSource = std::make_unique<NetworkVideoSource>();
        videoSource->start(ip.c_str(), port, [&output_file](const char* data, int size, int width, int height) {
            // Write data to file stream
            output_file.write(data, size);
            });

        // Start a thread to listen for user input
        while (true) {
            std::cout << "Press 'q' to exit the server." << std::endl;
            char ch = _getch();
            std::cout << "You pressed: " << ch << std::endl;
            if (ch == 'q' || ch == 'Q') { // Check if 'q' or 'Q' is pressed
                videoSource.reset();
                break;
            }
        }

        printf("All frames have been written to decoded_frames.yuv\n");
    }
    else if (mode == "c") {
        // Define client run function
        auto run = [&](CameraDataSender& sender) {
            // Global initialization (only call once)
            avdevice_register_all();

            // Create encoder instance
            H264Encoder h264_encoder(resolution_width, resolution_height, [&sender](const uint8_t* data, size_t size) {
                OutputDebugStringA((std::string("encode done send size: ") + std::to_string(size)).c_str());
                // Implement 4-byte big-endian length header, matching Java's ByteBuffer.putInt(length)
                std::vector<uint8_t> packet(4 + size);
                packet[0] = (size >> 24) & 0xFF;
                packet[1] = (size >> 16) & 0xFF;
                packet[2] = (size >> 8) & 0xFF;
                packet[3] = (size) & 0xFF;
                std::copy(data, data + size, packet.begin() + 4);
                sender.sendData(reinterpret_cast<const char*>(packet.data()), static_cast<uint32_t>(packet.size()));
                }, frameRate);

            // ZED Camera setup
            sl::Camera zed;
            sl::InitParameters init_parameters;
            // Set desired ZED resolution. Choose the closest or compatible one.
            if (resolution_width == 1920 && resolution_height == 1080) {
                init_parameters.camera_resolution = sl::RESOLUTION::HD1080;
            }
            else if (resolution_width == 1280 && resolution_height == 720) {
                init_parameters.camera_resolution = sl::RESOLUTION::HD720;
            }
            else if (resolution_width == 2208 && resolution_height == 1242) {
                init_parameters.camera_resolution = sl::RESOLUTION::HD2K;
            }
            else if (resolution_width == 672 && resolution_height == 376) {
                init_parameters.camera_resolution = sl::RESOLUTION::VGA;
            }
            else {
                std::cerr << "Warning: Requested resolution " << resolution_width << "x" << resolution_height
                    << " not directly supported by ZED SDK. Using HD720 as default." << std::endl;
                init_parameters.camera_resolution = sl::RESOLUTION::HD720;
            }
            init_parameters.camera_fps = frameRate;
            init_parameters.depth_mode = sl::DEPTH_MODE::NONE; // We only need RGB here

            // Open the camera
            sl::ERROR_CODE err = zed.open(init_parameters);
            if (err != sl::ERROR_CODE::SUCCESS) {
                std::cerr << "Error " << err << " opening ZED Camera" << std::endl;
                sender.disconnect();
                return;
            }

            // Verify ZED camera resolution against requested encoder resolution
            if (zed.getCameraInformation().camera_configuration.resolution.width != resolution_width ||
                zed.getCameraInformation().camera_configuration.resolution.height != resolution_height) {
                std::cerr << "Warning: ZED camera resolution ("
                    << zed.getCameraInformation().camera_configuration.resolution.width << "x"
                    << zed.getCameraInformation().camera_configuration.resolution.height
                    << ") does not exactly match requested encoder resolution ("
                    << resolution_width << "x" << resolution_height << "). "
                    << "Image might be scaled or distorted." << std::endl;
            }

            bool continue_capture = true;

            std::thread input_thread([&continue_capture]() {
                std::cout << "Press Q to stop capturing..." << std::endl;
                while (true) {
                    if (_kbhit()) {
                        char ch = _getch();
                        if (ch == 'q' || ch == 'Q') {
                            continue_capture = false;
                            break;
                        }
                    }
                    std::this_thread::sleep_for(std::chrono::milliseconds(100));
                }
                });

            sl::Mat zed_image; // ZED SDK's image format

            while (continue_capture) {
                if (zed.grab() == sl::ERROR_CODE::SUCCESS) {
                    // Retrieve image in RGBA format
                    zed.retrieveImage(zed_image, sl::VIEW::LEFT, sl::MEM::CPU);

                    // Convert sl::Mat (RGBA) to YUV (I420/YUV420p)
                    std::vector<uint8_t> y_plane, u_plane, v_plane;
                    convertRBGAToYUV(zed_image, y_plane, u_plane, v_plane);

                    // Pass YUV planes to the encoder
                    h264_encoder.encodeFrame(y_plane.data(), u_plane.data(), v_plane.data(),
                        y_plane.size(), u_plane.size(), v_plane.size());
                }
                else {
                    std::this_thread::sleep_for(std::chrono::milliseconds(1)); // Avoid busy-waiting
                }
            }

            zed.close(); // Close the ZED camera
            input_thread.join();
            // Disconnect
            sender.disconnect();
        };

        // Create client instance and start connection
        CameraDataSender sender(ip.c_str(), port);
        sender.startConnect(run); // Pass lambda function as callback
        sender.runIOContext(); // Run io_context to handle async operations

    }
    else {
        std::cout << "Invalid parameter, please use 's' or 'c'" << std::endl;
        return 1;
    }

    return 0;
}

// Global flag to signal application exit
bool app_should_quit = false;

// Function to print error and quit
void printErrorAndQuit(const std::string& errorMessage) {
    std::cerr << "Fatal Error: " << errorMessage << std::endl;
    app_should_quit = true; // Set flag for graceful shutdown if possible
    std::exit(EXIT_FAILURE); // Force exit
}

int runH264TCPCameraCaptureTest(int argc, char* argv[], const std::string& server_ip, int port, int resolution_width, int resolution_height, int frameRate, const std::string& camera_name, int64_t bitrate) {

    CameraDataSender sender(server_ip.c_str(), port);
    auto run = [&](CameraDataSender& sender) {
        avdevice_register_all();

        auto encoder_callback = [&sender](const uint8_t* data, size_t size) {
            if (size == 0 || data == nullptr) {
                OutputDebugStringA("Encoder callback received empty data.\n");
                return;
            }

            std::vector<uint8_t> packet(4 + size);

            // Write length in big-endian format
            packet[0] = (size >> 24) & 0xFF;
            packet[1] = (size >> 16) & 0xFF;
            packet[2] = (size >> 8) & 0xFF;
            packet[3] = (size) & 0xFF;

            // Copy NALU data
            std::copy(data, data + size, packet.begin() + 4);

            // --- Catch SendDataException here ---
            try {
                // Send the length-prefixed packet
                sender.sendData(reinterpret_cast<const char*>(packet.data()), static_cast<uint32_t>(packet.size()));
            }
            catch (const SendDataException& e) {
                printErrorAndQuit(e.what()); // Call the function to print error and quit
            }
            catch (const std::exception& e) { // Catch any other standard exceptions
                printErrorAndQuit("An unexpected error occurred during sendData: " + std::string(e.what()));
            }
        };

        // In SIDE_BY_SIDE mode, the real width is 2 * resolution_width
        H264Encoder h264_encoder(resolution_width * 2, resolution_height, encoder_callback, frameRate, bitrate);

        // ZED Camera setup
        sl::Camera zed;
        sl::InitParameters init_parameters;
        // Set desired ZED resolution. Choose the closest or compatible one.
        if (resolution_width == 1920 && resolution_height == 1080) {
            init_parameters.camera_resolution = sl::RESOLUTION::HD1080;
        }
        else if (resolution_width == 1280 && resolution_height == 720) {
            init_parameters.camera_resolution = sl::RESOLUTION::HD720;
        }
        else if (resolution_width == 2208 && resolution_height == 1242) {
            init_parameters.camera_resolution = sl::RESOLUTION::HD2K;
        }
        else if (resolution_width == 672 && resolution_height == 376) {
            init_parameters.camera_resolution = sl::RESOLUTION::VGA;
        }
        else {
            std::cerr << "Warning: Requested resolution " << resolution_width << "x" << resolution_height
                << " not directly supported by ZED SDK. Using HD720 as default." << std::endl;
            init_parameters.camera_resolution = sl::RESOLUTION::HD720;
        }
        init_parameters.camera_fps = frameRate;
        init_parameters.depth_mode = sl::DEPTH_MODE::NONE; // We only need RGB here

        // Open the camera
        sl::ERROR_CODE err = zed.open(init_parameters);
        if (err != sl::ERROR_CODE::SUCCESS) {
            std::cerr << "Error " << err << " opening ZED Camera" << std::endl;
            sender.disconnect();
            return;
        }

        // Verify ZED camera resolution against requested encoder resolution
        if (zed.getCameraInformation().camera_configuration.resolution.width != resolution_width ||
            zed.getCameraInformation().camera_configuration.resolution.height != resolution_height) {
            std::cerr << "Warning: ZED camera resolution ("
                << zed.getCameraInformation().camera_configuration.resolution.width << "x"
                << zed.getCameraInformation().camera_configuration.resolution.height
                << ") does not match requested encoder resolution ("
                << resolution_width << "x" << resolution_height << "). "
                << "Image might be scaled or distorted." << std::endl;
        }

        bool continue_capture = true;

        std::thread input_thread([&continue_capture]() {
            std::cout << "Press Q to stop capturing..." << std::endl;
            while (true) {
#ifdef _WIN32
                if (_kbhit()) {
                    char ch = _getch();
                    if (ch == 'q' || ch == 'Q') {
                        continue_capture = false;
                        break;
                    }
                }
#else
                // Simple non-blocking input for Linux/macOS
                char ch;
                if (std::cin.peek() != EOF && std::cin >> ch) {
                    if (ch == 'q' || ch == 'Q') {
                        continue_capture = false;
                        break;
                    }
                }
#endif
                std::this_thread::sleep_for(std::chrono::milliseconds(100));
                if (app_should_quit) { // Check global flag to exit input thread early
                    break;
                }
            }
            });

        sl::Mat zed_image; // ZED SDK's image format

        // Main capture loop. It will also check the global app_should_quit flag.
        while (continue_capture && !app_should_quit) {
            if (zed.grab() == sl::ERROR_CODE::SUCCESS) {
                // Retrieve image in RGBA format (compatible with OpenCV)
                zed.retrieveImage(zed_image, sl::VIEW::SIDE_BY_SIDE, sl::MEM::CPU);

                // Convert sl::Mat (RGBA) to YUV (I420/YUV420p)
                std::vector<uint8_t> y_plane, u_plane, v_plane;
                convertRBGAToYUV(zed_image, y_plane, u_plane, v_plane);

                // Pass YUV planes to the encoder
                // If an exception occurs in encoder_callback (due to sendData), app_should_quit will be set.
                h264_encoder.encodeFrame(y_plane.data(), u_plane.data(), v_plane.data(),
                    y_plane.size(), u_plane.size(), v_plane.size());
            }
            else {
                std::this_thread::sleep_for(std::chrono::milliseconds(1)); // Avoid busy-waiting
            }
        }

        zed.close(); // Close the ZED camera
        input_thread.join();
        sender.disconnect();
    };

    // The startConnect callback now also uses printErrorAndQuit
    sender.startConnect([&](CameraDataSender& s) {
        try {
            run(s);
        }
        catch (const std::exception& e) {
            // This catches any remaining unhandled exceptions from the 'run' lambda
            printErrorAndQuit("Unhandled exception in run lambda: " + std::string(e.what()));
        }
        });
    sender.runIOContext();
    return 0;
}


int analyzeDelay(int argc, char* argv[]) {
    // Specify the input log file (modify if needed)
    std::string inputFilename = "Messages.dblog";
    std::ifstream infile(inputFilename);
    if (!infile) {
        std::cerr << "Failed to open log file: " << inputFilename << std::endl;
        return 1;
    }

    // Define stage markers for the 6 process steps.
    std::vector<std::string> stages = {
        "get frame from camera",
        "get yuv420 frame",
        "encode done send size",
        "dataCallback decode input size",
        "frameCallback decode output size",
        "presentFrame done"
    };

    // Use separate vectors to store timestamps for each stage
    std::vector<double> stage0; // get frame from camera
    std::vector<double> stage1; // get yuv420 frame
    std::vector<double> stage2; // encode done send size
    std::vector<double> stage3; // dataCallback decode input size
    std::vector<double> stage4; // frameCallback decode output size
    std::vector<double> stage5; // presentFrame done

    std::string line;
    int lineCount = 0;
    // Read log file and save timestamps based on different stage keywords
    while (std::getline(infile, line)) {
        lineCount++;
        if (line.empty()) continue;
        std::istringstream iss(line);
        int seq;
        double timeStamp;
        int pid;
        std::string procName;
        // Parse first 4 columns: sequence number, timestamp, pid, process name
        if (!(iss >> seq >> timeStamp >> pid >> procName)) {
            std::cout << "Line " << lineCount << ": Failed to parse header." << std::endl;
            continue;
        }
        // Read remaining part as log message
        std::string message;
        std::getline(iss, message);
        size_t pos = message.find_first_not_of(" ");
        if (pos != std::string::npos) {
            message = message.substr(pos);
        }

        // Save timestamps based on different stage keywords (stages do not need to appear in order)
        if (message.find(stages[0]) != std::string::npos) {
            stage0.push_back(timeStamp);
        }
        if (message.find(stages[1]) != std::string::npos) {
            stage1.push_back(timeStamp);
        }
        if (message.find(stages[2]) != std::string::npos) {
            stage2.push_back(timeStamp);
        }
        if (message.find(stages[3]) != std::string::npos) {
            stage3.push_back(timeStamp);
        }
        if (message.find(stages[4]) != std::string::npos) {
            stage4.push_back(timeStamp);
        }
        if (message.find(stages[5]) != std::string::npos) {
            stage5.push_back(timeStamp);
        }
    }
    infile.close();

    // Output log line counts for debugging
    std::cout << "Total lines read: " << lineCount << std::endl;
    std::cout << "Stage 0 (get frame from camera) count: " << stage0.size() << std::endl;
    std::cout << "Stage 1 (get yuv420 frame) count: " << stage1.size() << std::endl;
    std::cout << "Stage 2 (encode done send size) count: " << stage2.size() << std::endl;
    std::cout << "Stage 3 (dataCallback decode input size) count: " << stage3.size() << std::endl;
    std::cout << "Stage 4 (frameCallback decode output size) count: " << stage4.size() << std::endl;
    std::cout << "Stage 5 (presentFrame done) count: " << stage5.size() << std::endl;

    // Use smallest count of log lines as complete cycle count
    size_t cycles = std::min({ stage0.size(), stage1.size(), stage2.size(), stage3.size(), stage4.size(), stage5.size() });
    std::cout << "Total complete cycles: " << cycles << std::endl;

    // Open CSV file for output.
    std::string outputFilename = "output.csv";
    std::ofstream outfile(outputFilename);
    if (!outfile) {
        std::cerr << "Failed to open CSV output file." << std::endl;
        return 1;
    }

    // Write CSV header with column titles
    outfile << "Capture,Encode,tcp transfer,Decode,Present\n";

    // Calculate and output stage durations to CSV file for each cycle
    for (size_t i = 0; i < cycles; i++) {
        double d1 = (stage1[i] - stage0[i]) * 1000;
        double d2 = (stage2[i] - stage1[i]) * 1000;
        double d3 = (stage3[i] - stage2[i]) * 1000;
        double d4 = (stage4[i] - stage3[i]) * 1000;
        double d5 = (stage5[i] - stage4[i]) * 1000;
        outfile << std::fixed << std::setprecision(6)
            << d1 << "," << d2 << "," << d3 << "," << d4 << "," << d5 << "\n";

    }

    // Compute and print average stage durations
    double total_d1 = 0.0, total_d2 = 0.0, total_d3 = 0.0, total_d4 = 0.0, total_d5 = 0.0;
    for (size_t i = 0; i < cycles; i++) {
        total_d1 += (stage1[i] - stage0[i]) * 1000;
        total_d2 += (stage2[i] - stage1[i]) * 1000;
        total_d3 += (stage3[i] - stage2[i]) * 1000;
        total_d4 += (stage4[i] - stage3[i]) * 1000;
        total_d5 += (stage5[i] - stage4[i]) * 1000;
    }
    double avg_d1 = (cycles > 0) ? total_d1 / cycles : 0;
    double avg_d2 = (cycles > 0) ? total_d2 / cycles : 0;
    double avg_d3 = (cycles > 0) ? total_d3 / cycles : 0;
    double avg_d4 = (cycles > 0) ? total_d4 / cycles : 0;
    double avg_d5 = (cycles > 0) ? total_d5 / cycles : 0;
    outfile << avg_d1 << "," << avg_d2 << "," << avg_d3 << "," << avg_d4 << "," << avg_d5 << "\n";

    outfile.close();
    std::cout << "CSV file generated: " << outputFilename << std::endl;

    return 0;
}

void printUsage(const char* programName) {
    std::cout << "Usage: " << programName << " [option] [parameters]" << std::endl;
    std::cout << "Options:" << std::endl;
    std::cout << "  --camera-test        Run camera capture, H.264 encoding and decoding test" << std::endl;
    std::cout << "                       Parameters: --camera <camera_name> --width <width> --height <height> --fps <fps>" << std::endl;
    std::cout << "  --simple-capture     Run simple camera capture test (save YUV file)" << std::endl;
    std::cout << "                       Parameters: --camera <camera_name> --width <width> --height <height> --fps <fps>" << std::endl;
    std::cout << "  --tcp-transfer [s|c] Run H.264 TCP transfer test" << std::endl;
    std::cout << "                       s: Server side" << std::endl;
    std::cout << "                       c: Client side" << std::endl;
    std::cout << "                       Parameters (for client): --ip <ip_address> --port <port> --camera <camera_name> --width <width> --height <height> --fps <fps>" << std::endl;
    std::cout << "                       Parameters (for server): --ip <ip_address> --port <port>" << std::endl;
    std::cout << "  --tcp-camera c       Run complete camera capture, H.264 encoding, TCP transfer test" << std::endl;
    std::cout << "                       c: Client side" << std::endl;
    std::cout << "                       Parameters (for client): --ip <ip_address> --port <port> --camera <camera_name> --width <width> --height <height> --fps <fps> --bitrate <bitrate>" << std::endl;
    std::cout << "                       Note: The server is located in the VideoPlayer." << std::endl;
    std::cout << "  --analyze-delay      Analyze delays in video processing stages" << std::endl;
    std::cout << "Default camera: video=Integrated Webcam" << std::endl;
    std::cout << "Default IP: 127.0.0.1" << std::endl;
    std::cout << "Default Port: 12345" << std::endl;
    std::cout << "Default Width: 1280" << std::endl;
    std::cout << "Default Height: 720" << std::endl;
    std::cout << "Default FPS: 30" << std::endl;
    std::cout << "Default Bitrate: 4000000 bps (4 Mbps)" << std::endl;
}

int main(int argc, char* argv[]) {
    if (argc < 2) {
        printUsage(argv[0]);
        return 1;
    }

    std::string option = argv[1];

    // Default values for parameters
    std::string camera_name = "video=Integrated Webcam"; // This will not be used by ZED camera directly
    int resolution_width = 1280;
    int resolution_height = 720;
    int frameRate = 30;
    std::string ip = "127.0.0.1";
    int port = 12345;
    int64_t bitrate = 4000000; // Default bitrate 4 Mbps

    // Parse command-line arguments for common parameters
    for (int i = 2; i < argc; ++i) {
        std::string arg = argv[i];
        if (arg == "--camera" && i + 1 < argc) {
            camera_name = argv[++i];
        }
        else if (arg == "--width" && i + 1 < argc) {
            resolution_width = std::stoi(argv[++i]);
        }
        else if (arg == "--height" && i + 1 < argc) {
            resolution_height = std::stoi(argv[++i]);
        }
        else if (arg == "--fps" && i + 1 < argc) {
            frameRate = std::stoi(argv[++i]);
        }
        else if (arg == "--ip" && i + 1 < argc) {
            ip = argv[++i];
        }
        else if (arg == "--port" && i + 1 < argc) {
            port = std::stoi(argv[++i]);
        }
        else if (arg == "--bitrate" && i + 1 < argc) {
            bitrate = std::stoll(argv[++i]);
        }
    }

    if (option == "--camera-test") {
        // For ZED, camera_name is not directly used as a device string, but resolution and framerate are.
        return RunCameraCaptureTest(const_cast<char*>(camera_name.c_str()), resolution_width, resolution_height, frameRate);
    }
    else if (option == "--simple-capture") {
        // For ZED, camera_name is not directly used as a device string, but resolution and framerate are.
        return RunSimpleCameraCaptureTest(const_cast<char*>(camera_name.c_str()), resolution_width, resolution_height, frameRate);
    }
    else if (option == "--tcp-transfer") {
        if (argc < 3) {
            std::cout << "Error: --tcp-transfer requires [s|c] parameter" << std::endl;
            printUsage(argv[0]);
            return 1;
        }
        // Pass the mode argument (argv[2]) to runH264TCPTransferTest
        return runH264TCPTransferTest(2, argv + 1, ip, port, resolution_width, resolution_height, frameRate, camera_name);
    }
    else if (option == "--tcp-camera") {
        if (argc < 3) {
            std::cout << "Error: --tcp-camera requires c parameter" << std::endl;
            printUsage(argv[0]);
            return 1;
        }
        // Pass the mode argument (argv[2]) to runH264TCPCameraCaptureTest
        return runH264TCPCameraCaptureTest(2, argv + 1, ip, port, resolution_width, resolution_height, frameRate, camera_name, bitrate);
    }
    else if (option == "--analyze-delay") {
        return analyzeDelay(argc - 1, argv + 1);
    }
    else {
        std::cout << "Error: Unknown option " << option << std::endl;
        printUsage(argv[0]);
        return 1;
    }
}