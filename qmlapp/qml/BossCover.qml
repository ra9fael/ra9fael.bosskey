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
    // Transparent window, opaque scenes: every scene paints its own fullscreen
    // background (the black hole paints a dark sky), so nothing of the desktop
    // shows through any of them.
    color: "transparent"

    // No `visible:` line here: a QML binding re-asserted false over the C++
    // showFullScreen (the window was created fullscreen but stayed invisible).
    // QML windows start hidden, and only BossQml.cpp shows or hides them.

    Loader {
        id: sceneLoader
        anchors.fill: parent
        sourceComponent: root.cover.scene === "bsod" ? bsodScene
                       : root.cover.scene === "update" ? updateScene
                       : root.cover.scene === "matrix" ? matrixScene
                       : root.cover.scene === "blackhole" ? blackholeScene
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
    Component {
        id: blackholeScene
        BlackholeScene { cover: root.cover; screenIndex: root.screenIndex }
    }

    // Keys cannot attach to a Window root ("is not an Item"), so a transparent
    // item on top of the scene takes the focus and feeds the matcher. Nothing
    // shows, whatever the boss sees you type.
    Item {
        id: inputTarget
        anchors.fill: parent
        focus: true
        Keys.onPressed: (event) => {
            if (root.cover.keyPressed(event.text, event.key))
                event.accepted = true
        }
        Component.onCompleted: forceActiveFocus()
    }

    // The windows are precreated hidden, so the initial focus request happens
    // before the window can become active. Restore it on every real show and
    // activation instead of requiring a click before QML receives keys.
    onVisibleChanged: {
        if (visible)
            inputTarget.forceActiveFocus()
    }
    onActiveChanged: {
        if (active)
            inputTarget.forceActiveFocus()
    }
}
