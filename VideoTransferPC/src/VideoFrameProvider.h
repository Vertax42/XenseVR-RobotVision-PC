//Video frame provider class for handling video frame presentation in Qt applications
#ifndef VIDEOFRAMEPROVIDER_H
#define VIDEOFRAMEPROVIDER_H

#include <QObject>
#include <QDebug>
#include <QVideoSink>
#include <QVideoFrame>
class VideoFrameProvider: public QObject
{
    Q_OBJECT

    Q_PROPERTY(QVideoSink *videoSurface READ videoSurface WRITE setVideoSurface NOTIFY videoSurfaceChanged)

public:
    explicit VideoFrameProvider(QObject *parent = nullptr);
    ~VideoFrameProvider();

    QVideoSink *videoSurface();
    void setVideoSurface(QVideoSink *surface);

    void presentFrame(const QByteArray &frameData, int width, int height, QVideoFrameFormat::PixelFormat pixFormat);

signals:
    void videoSurfaceChanged();

private:
    static void writeVideoFramePlane(uint8_t *dst, int dstlinesize, const uint8_t *src, int xsize, int ysize);
    void setFormat(int width, int height, QVideoFrameFormat::PixelFormat pixFormat);

private:
    QVideoSink *m_surface = nullptr;
    int m_width;
    int m_height;
    QVideoFrameFormat::PixelFormat m_pixFormat;
};

#endif // VIDEOFRAMEPROVIDER_H
