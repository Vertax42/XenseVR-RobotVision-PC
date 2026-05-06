//Color frame generator class for creating animated color patterns
#ifndef COLORFRAMEGENERATOR_H
#define COLORFRAMEGENERATOR_H

#include <QObject>
#include <QTimer>
#include <QVideoFrameFormat>
#include "VideoFrameProvider.h"
#include "SolidColorFrame.h"
#include <functional>
#include <cmath>

class ColorFrameGenerator : public QObject
{
    Q_OBJECT

public:
    explicit ColorFrameGenerator(QObject *parent, int frameWidth, int frameHeight);
    ~ColorFrameGenerator();

    void start(std::function<void(const char*, int, int, int)> presentFrameFunc);
    void stop();

    // Get frame size
    int frameWidth() { return m_frameWidth; }
    int frameHeight() { return m_frameHeight; }

private slots:
    void generateFrame();

private:
    QTimer* m_timer;
    float m_time;  // Time counter for sine value calculation
    int m_frameWidth;
    int m_frameHeight;

    // Frame presentation function
    std::function<void(const char*, int, int, int)> m_presentFrameLambda;
};

#endif // COLORFRAMEGENERATOR_H
