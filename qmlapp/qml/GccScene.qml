// GccScene.qml - the fake terminal: a fullscreen console streaming a gcc build
// of the configured project. The full log is generated once (logic.h, seeded)
// and revealed a line or two per tick; warnings get the amber tone, prompts
// the teal one.
import QtQuick

Rectangle {
    id: root

    required property var cover

    color: "#0C0C0C"

    ListView {
        id: view

        anchors.fill: parent
        anchors.margins: 14
        model: root.cover.gccModel
        clip: true
        interactive: false

        delegate: Text {
            width: view.width
            required property string line
            text: line
            color: line.indexOf("warning") >= 0 ? "#E5C07B"
                 : line.startsWith("$") ? "#4EC9B0"
                 : "#CCCCCC"
            font.family: "Consolas"
            font.pixelSize: Math.max(13, Math.round(root.height / 40))
        }

        onCountChanged: if (count > 0)
            positionViewAtIndex(count - 1, ListView.End)
    }

    Timer {
        running: root.cover.shown && !root.cover.gccModel.finished
        interval: 120
        repeat: true
        onTriggered: root.cover.gccModel.revealMore(1 + Math.floor(Math.random() * 2))
    }
}
