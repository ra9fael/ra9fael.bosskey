// BlackholeScene.qml - a Schwarzschild black hole. Every pixel's null geodesic
// is integrated in the fragment shader, so the shadow, the photon ring, the
// disk's upper and lower arcs and the starfield lensed into arcs around it are
// results, not drawings. Physics and porting notes: docs/blackhole-shader.md.
import QtQuick

Item {
    id: root

    required property var cover
    required property int screenIndex

    // Seconds of animation, advanced by the real elapsed time so the disk keeps
    // its wall-clock pace whatever the frame rate; frozen while the cover hides.
    property real t: 0
    property real lastTick: 0

    ShaderEffect {
        anchors.fill: parent

        property real uAspect: Math.max(0.0001, width / height)
        property real uTime: root.t
        property real uSeed: root.screenIndex * 0.618034
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
        property real uStarGain: 1.0
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
