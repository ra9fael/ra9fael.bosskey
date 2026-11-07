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

        // A little air, as the real screen has between the percent and the code.
        Item {
            width: 1
            height: Math.round(root.height * 0.03)
        }

        // The QR code, a real one: qrcode.h encodes the same stopcode URL the
        // text below prints, so scanning it opens that page. The white card is
        // the quiet zone (4 modules on every side, 29 + 8 = 37 across).
        Rectangle {
            width: Math.round(root.height * 0.11)
            height: width
            color: "white"

            Canvas {
                anchors.fill: parent
                anchors.margins: Math.round(parent.width * 4 / 37)
                onPaint: {
                    const ctx = getContext("2d")
                    const rows = root.cover.qrRows
                    const n = rows.length
                    if (n === 0)
                        return
                    ctx.fillStyle = "black"
                    const step = width / n
                    for (let r = 0; r < n; ++r) {
                        const line = rows[r]
                        for (let c = 0; c < n; ++c) {
                            if (line.charAt(c) !== "1")
                                continue
                            // Integer edges keep the modules crisp at this size.
                            const x0 = Math.round(c * step)
                            const x1 = Math.round((c + 1) * step)
                            const y0 = Math.round(r * step)
                            const y1 = Math.round((r + 1) * step)
                            ctx.fillRect(x0, y0, x1 - x0, y1 - y0)
                        }
                    }
                }
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
