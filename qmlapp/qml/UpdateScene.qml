// UpdateScene.qml - the Windows update screen: deep blue, the white ring
// spinner, the working-on-updates percent (stalled at 87 by the controller's
// pacing, like a real update), and the do-not-turn-off warning.
import QtQuick

Rectangle {
    id: root

    required property var cover

    color: "#003E92"

    Column {
        anchors.centerIn: parent
        spacing: Math.round(root.height * 0.05)

        Item {
            width: Math.round(root.height * 0.11)
            height: width
            anchors.horizontalCenter: parent.horizontalCenter

            Canvas {
                id: ring
                anchors.fill: parent
                onPaint: {
                    const ctx = getContext("2d")
                    ctx.reset()
                    ctx.strokeStyle = "#FFFFFF"
                    ctx.lineWidth = Math.max(3, width * 0.05)
                    ctx.lineCap = "round"
                    ctx.beginPath()
                    ctx.arc(width / 2, height / 2, width / 2 - ctx.lineWidth * 1.5, 0, Math.PI * 1.2)
                    ctx.stroke()
                }

                RotationAnimation on rotation {
                    loops: Animation.Infinite
                    from: 0
                    to: 360
                    duration: 1200
                    running: root.cover.shown
                }
            }
        }

        Text {
            anchors.horizontalCenter: parent.horizontalCenter
            color: "white"
            font.pixelSize: Math.round(root.height * 0.045)
            font.family: "Segoe UI"
            text: root.cover.zh
                  ? "正在处理更新 " + root.cover.percent + "%"
                  : "Working on updates " + root.cover.percent + "%"
        }

        Text {
            anchors.horizontalCenter: parent.horizontalCenter
            color: "#CCE0F5"
            font.pixelSize: Math.round(root.height * 0.032)
            font.family: "Segoe UI"
            text: root.cover.zh ? "请不要关闭电脑" : "Don't turn off your PC"
        }
    }

    Text {
        anchors.horizontalCenter: parent.horizontalCenter
        anchors.bottom: parent.bottom
        anchors.bottomMargin: root.height * 0.08
        color: "#8FB8E0"
        font.pixelSize: Math.round(root.height * 0.022)
        font.family: "Segoe UI"
        text: root.cover.zh ? "你的电脑将多次重启" : "Your PC will restart several times"
    }
}
