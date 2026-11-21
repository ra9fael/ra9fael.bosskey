// Fixed-centre control: production scene, baked shader and timing unchanged.
import QtQuick
import "../../build/cover-input-sky/control-component" as Scenes

Window {
    width: 480
    height: 270
    x: 0
    y: 0
    visible: true
    color: "#02040a"
    flags: Qt.FramelessWindowHint | Qt.WindowStaysOnTopHint | Qt.Tool
    title: "WinShunBossKeyCover"

    QtObject {
        id: state
        property bool shown: true
    }
    Scenes.BlackholeScene {
        id: scene
        anchors.fill: parent
        cover: state
        screenIndex: 0
        onWalkXChanged: { if (walkX !== 0) walkX = 0 }
        onWalkYChanged: { if (walkY !== 0) walkY = 0 }
        onVelocityXChanged: { if (velocityX !== 0) velocityX = 0 }
        onVelocityYChanged: { if (velocityY !== 0) velocityY = 0 }
    }
    Timer {
        interval: 22000
        running: true
        onTriggered: Qt.quit()
    }
}
