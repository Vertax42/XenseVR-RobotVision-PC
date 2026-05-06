//Video frame provider implementation for QML video playback
#include "VideoFrameProvider.h"
#include <Windows.h>
#include <iostream>
// Since all implementation is in the header file, this file can be empty
// If new implementation is needed, it can be added here

VideoFrameProvider::VideoFrameProvider(QObject *parent) : QObject(parent)
{
    m_width = 0;
    m_height = 0;
    m_pixFormat = QVideoFrameFormat::Format_Invalid;
}

VideoFrameProvider::~VideoFrameProvider()
{
}

QVideoSink *VideoFrameProvider::videoSurface()
{
    return m_surface;
}

void VideoFrameProvider::setVideoSurface(QVideoSink *surface)
{
    if (m_surface != surface)
    {
        m_surface = surface;
        emit videoSurfaceChanged();
    }
}

void VideoFrameProvider::writeVideoFramePlane(uint8_t *dst, int dstlinesize, const uint8_t *src, int xsize, int ysize)
{
    if(dstlinesize != xsize)
    {
        for(int i = 0; i<ysize; ++i)
        {
            memcpy(dst + i*dstlinesize, src + i*xsize, xsize);
        }
    }
    else
    {
        memcpy(dst, src, xsize*ysize);
    }
}

void VideoFrameProvider::presentFrame(const QByteArray &frameData, int width, int height, QVideoFrameFormat::PixelFormat pixFormat)
{
    if((width == m_width
        && height == m_height
        && pixFormat == m_pixFormat) == false)
    {
        std::cout << "reset format to" << frameData.size() << width << height << pixFormat << std::endl;
        setFormat(width, height, pixFormat);
    }

    QVideoFrame frame(QVideoFrameFormat(QSize(width, height), pixFormat));
    if (frame.map(QVideoFrame::WriteOnly))
    {
        switch(pixFormat)
        {
        case QVideoFrameFormat::Format_YUV420P:
        {
            writeVideoFramePlane(frame.bits(0), frame.bytesPerLine(0), (const uint8_t *)frameData.constData(), width, height);
            writeVideoFramePlane(frame.bits(1), frame.bytesPerLine(1), (const uint8_t *)frameData.constData() + width*height, width/2, height/2);
            writeVideoFramePlane(frame.bits(2), frame.bytesPerLine(2), (const uint8_t *)frameData.constData() + width*height + width*height/4, width/2, height/2);
        }
        break;
        case QVideoFrameFormat::Format_BGRA8888:
        {
            writeVideoFramePlane(frame.bits(0), frame.bytesPerLine(0), (const uint8_t *)frameData.constData(), width*4, height);
        }
        break;
        default:
        break;
        }

        frame.unmap();

        if (m_surface)
        {
            m_surface->setVideoFrame(frame);
            OutputDebugStringA("presentFrame done");
        }
    }
}

void VideoFrameProvider::setFormat(int width, int height, QVideoFrameFormat::PixelFormat pixFormat)
{
    m_width = width;
    m_height = height;
    m_pixFormat = pixFormat;
}

