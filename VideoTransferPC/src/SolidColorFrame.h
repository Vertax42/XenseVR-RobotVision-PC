//Class for generating solid color video frames in YUV format
#ifndef SOLIDCOLORFRAME_H
#define SOLIDCOLORFRAME_H

#include <QColor>

class SolidColorFrame
{
public:
    explicit SolidColorFrame(int width, int height, const QColor& color);
    ~SolidColorFrame();

    // Get frame data
    char* getFrameData() const { return m_frameBuffer; }
    int getFrameSize() const { return m_frameSize; }
    int getWidth() const { return m_width; }
    int getHeight() const { return m_height; }

private:
    void generateYUVFromRGB(int r, int g, int b);
    void initializeBuffers();
    void freeBuffers();

private:
    int m_width;
    int m_height;
    QColor m_color;

    // Frame data buffer
    char* m_frameBuffer;

    // Buffer size
    int m_ySize;
    int m_uvSize;
    int m_frameSize;
};

#endif // SOLIDCOLORFRAME_H
