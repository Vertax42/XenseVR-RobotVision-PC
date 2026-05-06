//H264 video decoder implementation using FFmpeg library
extern "C" {
#include <libavcodec/avcodec.h>
#include <libavutil/opt.h>
#include <libavutil/imgutils.h>
}

#include "H264Decoder.h"
#include <stdio.h>
#include <stdexcept>
#include "FFmpegUtils.h"
#include <memory>

void H264Decoder::cleanup() {
    if (m_pkt) {
        av_packet_free(&m_pkt);
        m_pkt = nullptr;
    }
    if (m_frame) {
        av_frame_free(&m_frame);
        m_frame = nullptr;
    }
    if (m_decCtx) {
        avcodec_free_context(&m_decCtx);
        m_decCtx = nullptr;
    }
    m_initialized = false;
}

H264Decoder::H264Decoder(DecodeCallback callback)
    : m_decCtx(nullptr), m_frame(nullptr), m_pkt(nullptr), m_initialized(false), m_callback(callback) {
    try {
        if (!callback) {
            throw std::runtime_error("Invalid callback function");
        }

        // Initialize decoder
        const AVCodec* codec = avcodec_find_decoder(AV_CODEC_ID_H264);
        if (!codec) {
            throw std::runtime_error("Failed to find H264 decoder");
        }

        // Allocate decoder context
        m_decCtx = avcodec_alloc_context3(codec);
        if (!m_decCtx) {
            throw std::runtime_error("Failed to allocate decoder context");
        }

        // Allocate frame buffer
        m_frame = av_frame_alloc();
        if (!m_frame) {
            throw std::runtime_error("Failed to allocate frame");
        }

        // Allocate packet
        m_pkt = av_packet_alloc();
        if (!m_pkt) {
            throw std::runtime_error("Failed to allocate packet");
        }

        // Open decoder
        if (avcodec_open2(m_decCtx, codec, nullptr) < 0) {
            throw std::runtime_error("Failed to open decoder");
        }

        m_initialized = true;
    }
    catch (const std::exception& e) {
        fprintf(stderr, "H264Decoder initialization failed: %s\n", e.what());
        cleanup();
        throw;
    }
}

H264Decoder::~H264Decoder() {
    cleanup();
}

void H264Decoder::decode(const uint8_t* data, size_t size) {
    if (!m_initialized || !data) {
        fprintf(stderr, "H264Decoder not initialized or invalid input data\n");
        return;
    }

    // Set packet data directly
    m_pkt->data = const_cast<uint8_t*>(data);
    m_pkt->size = size;

    // Send packet to decoder
    int send_ret = avcodec_send_packet(m_decCtx, m_pkt);
    if (send_ret < 0) {
        fprintf(stderr, "Error sending packet to decoder: %s\n", av_err2str_cpp(send_ret));
        return;
    }

    // Receive decoded frame
    while (true) {
        int recv_ret = avcodec_receive_frame(m_decCtx, m_frame);
        if (recv_ret == AVERROR(EAGAIN) || recv_ret == AVERROR_EOF) {
            break;
        }
        if (recv_ret < 0) {
            fprintf(stderr, "Error during decoding: %s\n", av_err2str_cpp(recv_ret));
            av_frame_unref(m_frame);
            break;
        }

        // Calculate YUV plane sizes after getting frame
        const int widths[3] = {m_frame->width, m_frame->width/2, m_frame->width/2};
        const int heights[3] = {m_frame->height, m_frame->height/2, m_frame->height/2};

        // Calculate total size and allocate buffer
        size_t totalSize = 0;
        for (int i = 0; i < 3; i++) {
            totalSize += widths[i] * heights[i];
        }
        auto buffer = std::make_unique<uint8_t[]>(totalSize);
        uint8_t* currentPos = buffer.get();

        // Copy data from three planes to continuous buffer
        for (int i = 0; i < 3; i++) {
            for (int j = 0; j < heights[i]; j++) {
                memcpy(currentPos, m_frame->data[i] + j * m_frame->linesize[i], widths[i]);
                currentPos += widths[i];
            }
        }

        // Call callback function to process data
        m_callback(buffer.get(), totalSize, m_frame->width, m_frame->height);

        av_frame_unref(m_frame);
    }
}
