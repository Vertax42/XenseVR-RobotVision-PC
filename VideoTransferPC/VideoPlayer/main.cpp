//Main entry point for the video player application, initializing QML and network video source
#include <QGuiApplication>
#include <QQmlApplicationEngine>
#include <QQmlContext>
#include "VideoFrameProvider.h"
#include "NetworkVideoSource.h"

int main(int argc, char* argv[])
{
    QGuiApplication app(argc, argv);

    // Default values
    QString ip = "127.0.0.1";
    int port = 12345;

    // Parse command line arguments for IP and Port
    for (int i = 1; i < argc; ++i) {
        QString arg = argv[i];
        if (arg == "--ip" && i + 1 < argc) {
            ip = argv[++i];
        }
        else if (arg == "--port" && i + 1 < argc) {
            port = QString(argv[++i]).toInt();
        }
    }

    QQmlApplicationEngine engine;

    // Create VideoFrameProvider instance and set as context property
    VideoFrameProvider* provider = new VideoFrameProvider(&engine);
    engine.rootContext()->setContextProperty("videoFrameProvider", provider);

    // Create and start network video source
    auto videoSource = std::make_unique<NetworkVideoSource>();
    videoSource->start(ip.toStdString().c_str(), port, [provider](const char* data, int size, int width, int height) {
        provider->presentFrame(QByteArray(data, size),
            width,
            height,
            QVideoFrameFormat::Format_YUV420P);
        });

    const QUrl url(u"qrc:/VideoPlayer/Main.qml"_qs);
    QObject::connect(
        &engine,
        &QQmlApplicationEngine::objectCreationFailed,
        &app,
        []() { QCoreApplication::exit(-1); },
        Qt::QueuedConnection);
    engine.load(url);

    // No need to manually delete after using smart pointers
    QObject::connect(&app, &QGuiApplication::aboutToQuit, [&videoSource]() {
        videoSource.reset();
        });

    return app.exec();
}