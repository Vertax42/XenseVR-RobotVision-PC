//Camera capture implementation using DirectShow for Windows platform
#include <windows.h>
extern "C" {
// Adjust these includes based on your FFmpeg installation path
#include <libavcodec/avcodec.h>
#include <libavformat/avformat.h>
#include <libavdevice/avdevice.h>
#include <libswscale/swscale.h>
#include <libavutil/imgutils.h>
}

#include "CameraCapture.h"
#include <stdio.h>
#include <vector>
#include <algorithm>
#include "FFmpegUtils.h"

void CameraCapture::custom_av_log(void* ptr, int level, const char* fmt, va_list vl) {
    char log_buffer[1024];
    vsnprintf(log_buffer, sizeof(log_buffer), fmt, vl);
    if (strstr(log_buffer, "unable to decode APP fields")) return;
    av_log_default_callback(ptr, level, fmt, vl);
}

void CameraCapture::defaultFrameCallback(const uint8_t* y, const uint8_t* u, const uint8_t* v,
                                       size_t y_size, size_t u_size, size_t v_size,
                                       int width, int height, int frame_index) {
    std::string filename = std::to_string(frame_index + 1) + ".yuv";
    FILE* frameFile = fopen(filename.c_str(), "wb");
    if (frameFile) {
        fwrite(y, 1, y_size, frameFile);
        fwrite(u, 1, u_size, frameFile);
        fwrite(v, 1, v_size, frameFile);
        fclose(frameFile);
        printf("Saved frame %d to %s\n", frame_index + 1, filename.c_str());
    } else {
        fprintf(stderr, "Failed to create frame file %s\n", filename.c_str());
    }
}

CameraCapture::CameraCapture(const char* device, const char* size, const char* rate,
                           const char* codec, int frames)
    : m_deviceName(device), m_videoSize(size), m_framerate(rate),
      m_codecName(codec), m_targetFrameCount(frames), m_shouldStop(false) {}

void CameraCapture::stopCapture() {
    m_shouldStop = true;
}

int CameraCapture::run(FrameCallback callback) {
    // Local variables declaration
    AVFormatContext* formatContext = nullptr;
    AVCodecContext* codecContext = nullptr;
    SwsContext* swsContext = nullptr;
    AVFrame* yuvFrame = nullptr;
    uint8_t* yuvBuffer = nullptr;
    AVPacket* packet = nullptr;
    AVFrame* frame = nullptr;
    AVDictionary* options = nullptr;
    int ret = 0;
    int videoStreamIndex = -1;

    // Need special handling variables
    AVCodecParameters* codecParams = nullptr;
    const AVCodec* codec = nullptr;
    AVPixelFormat src_format = AV_PIX_FMT_NONE;
    int yuvSize = 0;
    size_t frameSize = 0;
    int frameCount = 0;

    // Main logic
    av_log_set_callback(custom_av_log);

    // Device initialization
    const AVInputFormat* inputFormat = av_find_input_format("dshow");
    av_dict_set(&options, "video_size", m_videoSize.c_str(), 0);
    av_dict_set(&options, "framerate", m_framerate.c_str(), 0);
    av_dict_set(&options, "vcodec", m_codecName.c_str(), 0);

    // Add buffer parameters setting (unit: bytes)
    av_dict_set_int(&options, "rtbufsize", 100 * 1024 * 1024, 0);  // 100MB buffer
    av_dict_set(&options, "thread_queue_size", "512", 0);  // Increase thread queue size
    av_dict_set(&options, "use_wc_for_raw_video", "1", 0);  // Use more efficient buffer mechanism

    if ((ret = avformat_open_input(&formatContext, m_deviceName.c_str(), inputFormat, &options)) < 0) {
        fprintf(stderr, "Failed to open device: %s\n", av_err2str_cpp(ret));
        ret = -1;
        goto cleanup;
    }

    // Print active parameters
    {
        printf("Active Parameters:\n");
        AVDictionaryEntry* entry = nullptr;
        while ((entry = av_dict_get(options, "", entry, AV_DICT_IGNORE_SUFFIX))) {
            printf("  %s=%s\n", entry->key, entry->value);
        }
    }

    // Stream information parsing
    if (avformat_find_stream_info(formatContext, nullptr) < 0) {
        fprintf(stderr, "Failed to get stream info\n");
        ret = -1;
        goto cleanup;
    }

    // Find video stream
    for (unsigned int i = 0; i < formatContext->nb_streams; i++) {
        if (formatContext->streams[i]->codecpar->codec_type == AVMEDIA_TYPE_VIDEO) {
            videoStreamIndex = i;
            break;
        }
    }

    if (videoStreamIndex == -1) {
        fprintf(stderr, "Video stream not found\n");
        ret = -1;
        goto cleanup;
    }

    // get the decoder parameters
    codecParams = formatContext->streams[videoStreamIndex]->codecpar;

    // find the decoder
    codec = avcodec_find_decoder(codecParams->codec_id);
    if (!codec) {
        fprintf(stderr, "Decoder not found\n");
        ret = -1;
        goto cleanup;
    }

    // create the decoder context
    codecContext = avcodec_alloc_context3(codec);
    if (!codecContext) {
        fprintf(stderr, "Failed to create decoder context\n");
        ret = -1;
        goto cleanup;
    }

    // set the decoder parameters
    if (avcodec_parameters_to_context(codecContext, codecParams) < 0) {
        fprintf(stderr, "Failed to set decoder parameters\n");
        ret = -1;
        goto cleanup;
    }

    // open the decoder
    if (avcodec_open2(codecContext, codec, NULL) < 0) {
        fprintf(stderr, "Failed to open decoder\n");
        ret = -1;
        goto cleanup;
    }

    // After avcodec_open2()
    printf("Decoder actual parameters:\n");
    printf("  Format: %d (%s)\n", codecContext->pix_fmt, av_get_pix_fmt_name(codecContext->pix_fmt));
    printf("  Dimensions: %dx%d\n", codecContext->width, codecContext->height);

    // Stream information printing
    if (videoStreamIndex != -1) {
        AVStream* stream = formatContext->streams[videoStreamIndex];
        printf("\nVideo Stream Info:\n");
        printf("Codec ID: %d (%s)\n", stream->codecpar->codec_id, avcodec_get_name(stream->codecpar->codec_id));
        printf("Pixel Format: %d (%s)\n", codecContext->pix_fmt, av_get_pix_fmt_name(codecContext->pix_fmt));
        printf("Resolution: %dx%d\n", stream->codecpar->width, stream->codecpar->height);
        printf("Frame rate: %d/%d = %.2f fps\n", stream->avg_frame_rate.num, stream->avg_frame_rate.den, av_q2d(stream->avg_frame_rate));
    }

    // Creating SwsContext
    printf("\nCreating SwsContext with parameters:\n");
    printf("Source: %dx%d (%s)\n", codecContext->width, codecContext->height, av_get_pix_fmt_name(codecContext->pix_fmt));
    printf("Destination: %dx%d (%s)\n", codecContext->width, codecContext->height, av_get_pix_fmt_name(AV_PIX_FMT_YUV420P));

    if (codecContext->width % 2 != 0 || codecContext->height % 2 != 0) {
        fprintf(stderr, "ERROR: Resolution %dx%d is not compatible with YUV420P\n", codecContext->width, codecContext->height);
        ret = -1;
        goto cleanup;
    }

    // Simplified: directly use codecContext->pix_fmt as source format
    src_format = codecContext->pix_fmt;

    swsContext = sws_getContext(
        codecContext->width,
        codecContext->height,
        src_format,
        codecContext->width,
        codecContext->height,
        AV_PIX_FMT_YUV420P,
        SWS_BILINEAR | SWS_FULL_CHR_H_INT,
        NULL, NULL, NULL
    );

    if (!swsContext) {
        fprintf(stderr, "Failed to create SwsContext.\n");
        ret = -1;
        goto cleanup;
    } else {
        const int src_range = codecContext->color_range == AVCOL_RANGE_JPEG;
        sws_setColorspaceDetails(swsContext,
                                 sws_getCoefficients(SWS_CS_ITU709), src_range,
                                 sws_getCoefficients(SWS_CS_ITU709), 0,
                                 0, 1 << 16, 1 << 16);
    }

    // Allocate YUV frame buffer
    yuvFrame = av_frame_alloc();
    yuvSize = av_image_get_buffer_size(AV_PIX_FMT_YUV420P, codecContext->width,
                                         codecContext->height, 1);
    yuvBuffer = (uint8_t*)av_malloc(yuvSize);
    av_image_fill_arrays(yuvFrame->data, yuvFrame->linesize, yuvBuffer,
                        AV_PIX_FMT_YUV420P, codecContext->width, codecContext->height, 1);

    // read and process frames
    packet = av_packet_alloc();
    frame = av_frame_alloc();
    frameCount = 0;

    // calculate the frame size once
    frameSize = static_cast<size_t>(codecContext->width) * static_cast<size_t>(codecContext->height);

    // Main decoding loop
    while ((m_targetFrameCount > 0 && frameCount < m_targetFrameCount) ||
           (m_targetFrameCount == 0 && !m_shouldStop)) {
        OutputDebugStringA((std::string("get frame from camera ") + std::to_string(frameCount)).c_str());
        if ((ret = av_read_frame(formatContext, packet)) < 0) {
            fprintf(stderr, "av_read_frame returned %d: %s\n", ret, av_err2str_cpp(ret));
            break;
        }

        if (packet->stream_index == videoStreamIndex) {
            if ((ret = avcodec_send_packet(codecContext, packet)) < 0) {
                fprintf(stderr, "avcodec_send_packet failed: %s\n", av_err2str_cpp(ret));
            } else {
                while (1) {
                    ret = avcodec_receive_frame(codecContext, frame);
                    if (ret == AVERROR(EAGAIN) || ret == AVERROR_EOF) {
                        // Add frame reference release
                        av_frame_unref(frame);
                        if (ret == AVERROR_EOF) {
                            printf("Decoder flushed all frames\n");
                        }
                        break;
                    }
                    if (ret < 0) {
                        fprintf(stderr, "avcodec_receive_frame error: %s\n", av_err2str_cpp(ret));
                        av_frame_unref(frame);
                        break;
                    }

                    // Simplified sws_scale call without checking slice height
                    sws_scale(swsContext,
                              frame->data, frame->linesize,
                              0, frame->height,
                              yuvFrame->data, yuvFrame->linesize);
                    OutputDebugStringA((std::string("get yuv420 frame ") + std::to_string(frameCount)).c_str());
                    callback(yuvFrame->data[0], yuvFrame->data[1], yuvFrame->data[2],
                             frameSize, frameSize / 4, frameSize / 4,
                             codecContext->width, codecContext->height,
                             frameCount);

                    ++frameCount;
                    av_frame_unref(frame);
                }
            }
        }
        av_packet_unref(packet);
    }

    // Add simple check after normal decoding loop
    if (codecContext->codec->capabilities & AV_CODEC_CAP_DELAY) {
        // Only flush for codecs that support delay
        avcodec_send_packet(codecContext, NULL);
        while (avcodec_receive_frame(codecContext, frame) >= 0) {
            // Process remaining frames...
            av_frame_unref(frame);
        }
    }

    if (frameCount < m_targetFrameCount) {
        ret = -1;
    } else {
        ret = 0;
        printf("Done! Processed %d frames\n", frameCount);
    }

cleanup: // Resource cleanup
    av_log_set_callback(av_log_default_callback);
    av_frame_free(&yuvFrame);
    av_frame_free(&frame);
    av_packet_free(&packet);
    av_free(yuvBuffer);
    av_dict_free(&options);
    sws_freeContext(swsContext);
    swsContext = nullptr;  // Explicitly set pointer to null

    avcodec_free_context(&codecContext);
    avformat_close_input(&formatContext);

    // Print debug information
    printf("Resource cleanup completed:\n");
    printf("FormatContext: %p\n", (void*)formatContext);
    printf("CodecContext: %p\n", (void*)codecContext);
    printf("SwsContext: %p\n", (void*)swsContext);

    return ret;
}
