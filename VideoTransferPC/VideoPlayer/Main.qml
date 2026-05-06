import QtQuick

Window {
    width: 800
    height: 600
    visible: true
    title: qsTr("Video Player Demo")

    VideoPlayer {
        width: 640
        height: 480
        anchors.centerIn: parent
    }
}
