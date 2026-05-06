//H264 video encoder implementation using FFmpeg library
#ifdef __cplusplus
extern "C" {
#endif
#include <libavcodec/avcodec.h>
#include <libavutil/opt.h>
#include <libavutil/imgutils.h>
#ifdef __cplusplus
}
#endif

#include "H264Encoder.h"
#include <stdio.h>
#include <algorithm>
#include <inttypes.h>
#include "FFmpegUtils.h"

H264Encoder::H264Encoder(int width, int height, AVpacketWriteCallback writeCallback, int fps, int64_t bitrate)
    : m_encCtx(nullptr), m_frame(nullptr), m_pkt(nullptr), m_writeCallback(writeCallback),
      m_ptsCounter(0)
{
    // Move all variable declarations to function start
    const AVCodec* codec = nullptr;
    int ret = 0;  // Declare early

    // Step 1: Find encoder
    codec = avcodec_find_encoder(AV_CODEC_ID_H264);
    if (!codec) {
        fprintf(stderr, "Codec not found\n");
        goto cleanup;
    }

    // Step 2: Allocate codec context
    m_encCtx = avcodec_alloc_context3(codec);
    if (!m_encCtx) {
        fprintf(stderr, "Failed to allocate codec context\n");
        goto cleanup;
    }

    // Step 3: Allocate frame
    m_frame = av_frame_alloc();
    if (!m_frame) {
        fprintf(stderr, "Failed to allocate frame\n");
        goto cleanup;
    }

    // Set frame parameters
    m_frame->format = AV_PIX_FMT_YUV420P;
    m_frame->width  = width;
    m_frame->height = height;

    // Allocate actual data buffer
    ret = av_frame_get_buffer(m_frame, 32);
    if (ret < 0) {
        fprintf(stderr, "Cannot allocate frame data: %s\n", av_err2str_cpp(ret));
        goto cleanup;
    }

    // Step 5: Allocate packet
    m_pkt = av_packet_alloc();
    if (!m_pkt) {
        fprintf(stderr, "Failed to allocate packet\n");
        goto cleanup;
    }

    // Step 6: Configure encoder parameters
    m_encCtx->width = width;
    m_encCtx->height = height;
    m_encCtx->time_base = AVRational{1, fps};
    m_encCtx->framerate = AVRational{fps, 1};

    // Use provided bitrate
    m_encCtx->bit_rate = bitrate;

    m_encCtx->gop_size = fps * 2; // 2 seconds per GOP
    m_encCtx->max_b_frames = 0;   // Disable B-frames
    m_encCtx->pix_fmt = AV_PIX_FMT_YUV420P;

    // Print encoder parameters
    printf("Encoder parameters:\n");
    printf("Resolution: %dx%d\n", m_encCtx->width, m_encCtx->height);
    printf("Time base: %d/%d\n", m_encCtx->time_base.num, m_encCtx->time_base.den);
    printf("Framerate: %d/%d\n", m_encCtx->framerate.num, m_encCtx->framerate.den);
    printf("Bitrate: %" PRId64 "\n", m_encCtx->bit_rate);
    printf("GOP size: %d\n", m_encCtx->gop_size);
    printf("B-frames: %d\n", m_encCtx->max_b_frames);
    printf("Pixel format: %s\n", av_get_pix_fmt_name(m_encCtx->pix_fmt));

    // Set H.264 preset parameters
    av_opt_set(m_encCtx->priv_data, "preset", "ultrafast", 0);
    av_opt_set(m_encCtx->priv_data, "tune", "zerolatency", 0);
    av_opt_set(m_encCtx->priv_data, "profile", "baseline", 0);
    av_opt_set(m_encCtx->priv_data, "annexb", "1", 0);
    av_opt_set(m_encCtx->priv_data, "sc_threshold", "0", 0);

    // Step 7: Open encoder
    if (avcodec_open2(m_encCtx, codec, nullptr) < 0) {
        fprintf(stderr, "Failed to open encoder\n");
        goto cleanup;
    }

    return; // Successful return

cleanup:
    // Release allocated resources in reverse order
    if (m_pkt) av_packet_free(&m_pkt);
    if (m_frame) av_frame_free(&m_frame);
    if (m_encCtx) avcodec_free_context(&m_encCtx);
}

void H264Encoder::encodeFrame(const uint8_t* y, const uint8_t* u, const uint8_t* v,
                            size_t y_size, size_t u_size, size_t v_size) {
    if (!m_encCtx || !m_frame || !m_pkt || !m_writeCallback) {
        fprintf(stderr, "Encoder not initialized\n");
        return;
    }

    // Fill YUV data
    memcpy(m_frame->data[0], y, y_size);
    memcpy(m_frame->data[1], u, u_size);
    memcpy(m_frame->data[2], v, v_size);
    m_frame->pts = m_ptsCounter++;

    // Send frame to encoder
    if (avcodec_send_frame(m_encCtx, m_frame) < 0) {
        fprintf(stderr, "Error sending frame to encoder\n");
        return;
    }

    while (1) {
        int ret = avcodec_receive_packet(m_encCtx, m_pkt);
        if (ret == AVERROR(EAGAIN) || ret == AVERROR_EOF)
            break;
        if (ret < 0) {
            fprintf(stderr, "Error during encoding\n");
            break;
        }

        m_writeCallback(m_pkt->data, m_pkt->size);
        av_packet_unref(m_pkt);
    }
}

void H264Encoder::finalize() {
    printf("Finalizing encoder and flushing remaining frames...\n");
    int ret = avcodec_send_frame(m_encCtx, nullptr);
    if (ret < 0) {
        fprintf(stderr, "Error sending null frame to flush encoder\n");
        return;
    }

    while (1) {
        ret = avcodec_receive_packet(m_encCtx, m_pkt);
        if (ret == AVERROR(EAGAIN) || ret == AVERROR_EOF)
            break;
        if (ret < 0) {
            fprintf(stderr, "Error receiving packet during flush\n");
            break;
        }

        m_writeCallback(m_pkt->data, m_pkt->size);
        av_packet_unref(m_pkt);
    }
    printf("Encoder finalization complete\n");
}

H264Encoder::~H264Encoder() {
    finalize();
    if (m_pkt) av_packet_free(&m_pkt);
    if (m_frame) av_frame_free(&m_frame);
    if (m_encCtx) avcodec_free_context(&m_encCtx);
}
