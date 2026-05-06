//Color frame generator implementation for testing video playback
#include "ColorFrameGenerator.h"
#include <QColor>
#include <memory>  // Add smart pointer header file

ColorFrameGenerator::ColorFrameGenerator(QObject *parent, int frameWidth, int frameHeight)
    : QObject(parent)
    , m_timer(new QTimer(this))
    , m_time(0.0f)
    , m_frameWidth(frameWidth)
    , m_frameHeight(frameHeight)
{
    m_timer->setInterval(33);  // About 30Hz refresh rate
    connect(m_timer, &QTimer::timeout, this, &ColorFrameGenerator::generateFrame);
}

ColorFrameGenerator::~ColorFrameGenerator()
{
    stop();
}

void ColorFrameGenerator::start(std::function<void(const char*, int, int, int)> presentFrameFunc)
{
    m_time = 0.0f;
    m_presentFrameLambda = presentFrameFunc;
    m_timer->start();
}

void ColorFrameGenerator::stop()
{
    m_timer->stop();
}

void ColorFrameGenerator::generateFrame()
{
    // Use cosine function to generate smooth grayscale value changes
    // cos value range is [-1,1], map it to [0,255]
    float grayValue = (std::cos(m_time) + 1.0f) * 127.5f;

    // Create frame using current grayscale value
    SolidColorFrame frame(m_frameWidth, m_frameHeight, QColor(static_cast<int>(grayValue),
                                                            static_cast<int>(grayValue),
                                                            static_cast<int>(grayValue)));

    // Present frame using member lambda function
    m_presentFrameLambda(frame.getFrameData(), frame.getFrameSize(), m_frameWidth, m_frameHeight);

    // Update time to control gradient speed
    m_time += 0.05f;  // This value can be adjusted to change gradient speed
}
