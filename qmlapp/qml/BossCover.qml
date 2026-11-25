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
    property bool aquariumSpanScreens: false
    property real aquariumScale: 1
    property int aquariumScreenCount: 1
    property bool aquariumCanCycleLayout: false

    flags: Qt.FramelessWindowHint | Qt.WindowStaysOnTopHint | Qt.Tool
    title: "WinShunBossKeyCover" // the verify scripts find it by this
    // Keep the native surface opaque. Scenes still paint their own backgrounds,
    // but an opaque window prevents stale desktop pixels during scene swaps and
    // while the Canvas backing store is being updated.
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
                       : root.cover.scene === "blackhole" ? blackholeScene
                       : root.cover.scene === "aquarium" ? aquariumScene
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
    Component {
        id: aquariumScene
        AquariumScene {
            cover: root.cover
            screenIndex: root.screenIndex
            spanScreens: root.aquariumSpanScreens
            spanScale: root.aquariumScale
            spanMonitorCount: root.aquariumScreenCount
        }
    }

    // Keys cannot attach to a Window root ("is not an Item"), so a transparent
    // item on top of the scene takes the focus and feeds the matcher.
    Item {
        id: inputTarget
        anchors.fill: parent
        focus: true
        Keys.onPressed: (event) => {
            if (root.cover.scene === "aquarium"
                    && (event.key === Qt.Key_Left || event.key === Qt.Key_Right)) {
                if (!event.isAutoRepeat && sceneLoader.status === Loader.Ready && sceneLoader.item)
                    sceneLoader.item.moveTitleHorizontally(event.key === Qt.Key_Left ? -1 : 1)
                event.accepted = true
                return
            }
            if (root.cover.scene === "aquarium"
                    && (event.key === Qt.Key_Tab || event.key === Qt.Key_Backtab)) {
                if (!event.isAutoRepeat && sceneLoader.status === Loader.Ready && sceneLoader.item) {
                    const reverse = event.key === Qt.Key_Backtab
                            || (event.modifiers & Qt.ShiftModifier) !== 0
                    if (root.aquariumCanCycleLayout && (event.modifiers & Qt.ControlModifier))
                        root.cover.cycleAquariumLayout(reverse ? -1 : 1)
                    else
                        sceneLoader.item.cycleEffect(reverse ? -1 : 1)
                }
                event.accepted = true
                return
            }
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
