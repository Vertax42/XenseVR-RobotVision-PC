import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import QtMultimedia

Rectangle {
    id: root
    color: "black"

    // Video output
    VideoOutput {
        id: videoOutput
        anchors.fill: parent
        fillMode: VideoOutput.Stretch
    }

    // Component initialization
    Component.onCompleted: {
        // Set videoSurface
        videoFrameProvider.videoSurface = videoOutput.videoSink;
        console.log("VideoPlayer component created")
    }

    // Component destruction
    Component.onDestruction: {
        console.log("VideoPlayer component destroyed")
    }
}
