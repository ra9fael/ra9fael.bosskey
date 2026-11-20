// BlackholeScene.qml - a Schwarzschild black hole on a transparent cover: the
// desktop behind is lensed around the hole, the shadow swallows it, and the
// accretion disk lights the scene. Every pixel's null geodesic is integrated in
// the fragment shader. Physics and porting notes: docs/blackhole-shader.md.
import QtQuick

Item {
    id: root

    required property var cover
    required property int screenIndex

    // Seconds of animation, advanced by the real elapsed time so the disk keeps
    // its wall-clock pace whatever the frame rate; frozen while the cover hides.
    property real t: 0
    property real lastTick: 0

    // The desktop snapshot the shader lenses. Invisible: only its texture is
    // sampled, never drawn. The epoch in the URL makes it reload when a new
    // snapshot arrives (the window is precreated, long before any snapshot).
    Image {
        id: desktop
        anchors.fill: parent
        source: "image://desktop/" + root.cover.desktopEpoch + "/" + root.screenIndex
        cache: false
        visible: false
    }

    ShaderEffect {
        anchors.fill: parent

        property real uAspect: Math.max(0.0001, width / height)
        property real uTime: root.t
        property real uSeed: root.screenIndex * 0.618034
        property variant uDesktop: desktop
        property real uHoleRadius: 0.125
        property real uDiskInner: 1.8
        property real uDiskOuter: 8.0
        property real uIncl: 1.45
        property real uRoll: 0.35
        property real uTemp: 4200
        property real uGain: 1.1
        property real uExposure: 1.0
        property real uSpeed: 5.0
        property real uWind: 7.0
        property real uContrast: 1.6
        property real uDoppler: 0.6
        property real uBeam: 2.2
        property real uOpacity: 0.9
        property real uDilation: 0.35

        fragmentShader: "BlackholeScene.frag.qsb"
    }

    // 30 fps: the cover can hang for minutes, and the picture is a slow drift.
    Timer {
        running: root.cover.shown
        interval: 33
        repeat: true
        onTriggered: {
            const now = Date.now()
            root.t += Math.min(0.25, (now - root.lastTick) / 1000)
            root.lastTick = now
        }
    }

    Component.onCompleted: root.lastTick = Date.now()
}
