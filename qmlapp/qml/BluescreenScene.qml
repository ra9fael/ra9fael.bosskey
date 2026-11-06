// BluescreenScene.qml - the iconic Windows 10 blue screen, left-aligned like
// the real one: the sad face, the collecting-errors text, the percent, the QR
// placeholder and the stop code. The percent comes from the controller's
// pacing (logic.h), so it stalls and crawls like the real thing.
import QtQuick

Rectangle {
    id: root

    required property var cover

    color: "#0078D7"

    // A stop code from the real pool, fixed for the cover's lifetime.
    property string stopCode: {
        const codes = ["CRITICAL_PROCESS_DIED", "WHEA_UNCORRECTABLE_ERROR",
                       "KERNEL_SECURITY_CHECK_FAILURE", "IRQL_NOT_LESS_OR_EQUAL",
                       "PAGE_FAULT_IN_NONPAGED_AREA", "SYSTEM_SERVICE_EXCEPTION"]
        return codes[Math.floor(Math.random() * codes.length)]
    }

    property int margin: Math.min(width, height) * 0.1
    property string whatFailed: root.cover.zh ? "winshun-core.exe" : "winshun-core.exe"

    Column {
        x: root.margin
        y: (root.height - height) * 0.34
        spacing: Math.round(root.height * 0.022)

        Text {
            text: ":("
            color: "white"
            font.pixelSize: Math.round(root.height * 0.16)
            font.family: "Microsoft YaHei"
        }

        Text {
            width: Math.min(root.width * 0.6, 720)
            color: "white"
            font.pixelSize: Math.round(root.height * 0.030)
            font.family: "Microsoft YaHei"
            wrapMode: Text.WordWrap
            text: root.cover.zh
                  ? "你的电脑遇到问题，需要重新启动。我们只收集某些错误信息，然后将为你重新启动。"
                  : "Your PC ran into a problem and needs to restart. We're just collecting some error info, and then we'll restart for you."
        }

        Text {
            color: "white"
            font.pixelSize: Math.round(root.height * 0.030)
            font.family: "Microsoft YaHei"
            text: root.cover.percent + "%"
        }

        Text {
            color: "white"
            font.pixelSize: Math.round(root.height * 0.024)
            font.family: "Microsoft YaHei"
            text: root.cover.zh ? "完成" : "complete"
        }
    }

    // The QR placeholder: a white square with a grid, far enough from the real
    // one to stay honest but near enough to read as it.
    Rectangle {
        x: root.margin
        y: root.height - root.margin * 0.4 - height * 3.1
        width: Math.round(root.height * 0.09)
        height: width
        color: "white"

        Canvas {
            anchors.fill: parent
            anchors.margins: parent.width * 0.12
            onPaint: {
                const ctx = getContext("2d")
                ctx.fillStyle = "#0078D7"
                const cells = 9
                const step = width / cells
                for (let i = 0; i < cells; ++i)
                    for (let j = 0; j < cells; ++j)
                        if ((i * 7 + j * 5 + i * j) % 3 === 0)
                            ctx.fillRect(i * step, j * step, step * 0.8, step * 0.8)
            }
        }
    }

    Column {
        x: root.margin
        y: root.height - root.margin * 0.4 - height
        spacing: Math.round(root.height * 0.012)

        Text {
            color: "white"
            font.pixelSize: Math.round(root.height * 0.020)
            font.family: "Microsoft YaHei"
            text: root.cover.zh
                  ? "有关此问题的详细信息和可能的修复方法，请访问 https://www.windows.com/stopcode"
                  : "For more information about this issue and possible fixes, visit https://www.windows.com/stopcode"
        }

        Text {
            color: "white"
            font.pixelSize: Math.round(root.height * 0.020)
            font.family: "Microsoft YaHei"
            text: root.cover.zh
                  ? "如果致电支持人员，请向他们提供以下信息：\n停止代码：" + root.stopCode + "\n失败的内容：" + root.whatFailed
                  : "If you call a support person, give them this info:\nStop code: " + root.stopCode + "\nWhat failed: " + root.whatFailed
        }
    }
}
