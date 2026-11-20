// GccScene.qml - the fake terminal: a fullscreen console streaming a gcc build
// of the configured project. The log never ends (GccModel appends build pass
// after pass), so the stream runs for as long as the cover is up and never
// reaches 100%. Warnings get the amber tone, prompts the teal one; long lines
// wrap the way a real terminal does.
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
            // A terminal never scrolls sideways: the rest of the line continues
            // on the next row.
            wrapMode: Text.WrapAnywhere
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
        running: root.cover.shown
        interval: 400
        repeat: true
        onTriggered: root.cover.gccModel.revealMore(Math.random() < 0.25 ? 2 : 1)
    }
}
