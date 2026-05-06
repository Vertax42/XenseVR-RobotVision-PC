//Solid color frame implementation for generating test video frames
#include "SolidColorFrame.h"

SolidColorFrame::SolidColorFrame(int width, int height, const QColor& color)
    : m_width(width)
    , m_height(height)
    , m_color(color)
    , m_frameBuffer(nullptr)
{
    // Calculate buffer size
    m_ySize = width * height;
    m_uvSize = (width * height) / 4;
    m_frameSize = m_ySize + 2 * m_uvSize;

    // Initialize buffer
    initializeBuffers();
}

SolidColorFrame::~SolidColorFrame()
{
    freeBuffers();
}

void SolidColorFrame::initializeBuffers()
{
    // Allocate memory
    m_frameBuffer = new char[m_frameSize];

    // Generate YUV data
    generateYUVFromRGB(m_color.red(), m_color.green(), m_color.blue());
}

void SolidColorFrame::freeBuffers()
{
    delete[] m_frameBuffer;
    m_frameBuffer = nullptr;
}

void SolidColorFrame::generateYUVFromRGB(int r, int g, int b)
{
    // YUV420P format conversion
    // Y component directly written to frame buffer start position
    for (int i = 0; i < m_width * m_height; i++) {
        m_frameBuffer[i] = (char)(0.299 * r + 0.587 * g + 0.114 * b);
    }

    // U component written after Y component
    for (int i = 0; i < (m_width * m_height) / 4; i++) {
        m_frameBuffer[m_ySize + i] = (char)(-0.147 * r - 0.289 * g + 0.436 * b + 128);
    }

    // V component written after U component
    for (int i = 0; i < (m_width * m_height) / 4; i++) {
        m_frameBuffer[m_ySize + m_uvSize + i] = (char)(0.615 * r - 0.515 * g - 0.100 * b + 128);
    }
}
