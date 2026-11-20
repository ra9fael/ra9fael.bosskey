// BossCover.qml - the cover window: frameless, always on top, outside the
// taskbar and alt-tab (Qt.Tool). One instance per screen; the controller is
// shared. Every key goes to the exit-sequence matcher, nothing shows.
import QtQuick

Window {
    id: root

    // Not `required`: createWithInitialProperties cannot attach required
    // properties to a Window root ("is not an Item"), a plain one it sets fine.
    property var cover
    property int screenIndex: 0

    flags: Qt.FramelessWindowHint | Qt.WindowStaysOnTopHint | Qt.Tool
    title: "WinShunBossKeyCover" // the verify scripts find it by this
    color: "#000000"

    // No `visible:` line here: a QML binding re-asserted false over the C++
    // showFullScreen (the window was created fullscreen but stayed invisible).
    // QML windows start hidden, and only BossQml.cpp shows or hides them.

    Loader {
        id: sceneLoader
        anchors.fill: parent
        sourceComponent: root.cover.scene === "bsod" ? bsodScene
                       : root.cover.scene === "update" ? updateScene
                       : root.cover.scene === "matrix" ? matrixScene
                       : gccScene
    }

    Component {
        id: bsodScene
        BluescreenScene { cover: root.cover }
    }
    Component {
        id: updateScene
        UpdateScene { cover: root.cover }
    }
    Component {
        id: gccScene
        GccScene { cover: root.cover }
    }
    Component {
        id: matrixScene
        MatrixScene { cover: root.cover; screenIndex: root.screenIndex }
    }

    // Keys cannot attach to a Window root ("is not an Item"), so a transparent
    // item on top of the scene takes the focus and feeds the matcher. Nothing
    // shows, whatever the boss sees you type.
    Item {
        anchors.fill: parent
        focus: true
        Keys.onPressed: (event) => {
            if (root.cover.keyPressed(event.text, event.key))
                event.accepted = true
        }
        Component.onCompleted: forceActiveFocus()
    }
}
