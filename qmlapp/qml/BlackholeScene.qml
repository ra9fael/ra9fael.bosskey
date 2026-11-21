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
    // its wall-clock pace whatever the frame rate; frozen while the cover hides
    // and rewound on every show, so the arrival plays again.
    property real t: 0
    property real lastTick: 0

    // The arrival: the cover opens on the plain desktop — as if every window had
    // been minimised — and then the hole grows from below a pixel to full size
    // while the disk heats up and brightens. Smootherstep, so it is born slowly,
    // opens fast and settles instead of stopping dead at full size.
    readonly property real introHold: 0.6  // seconds of untouched desktop
    readonly property real introGrow: 5.0  // seconds from a point to full size
    readonly property real grow: {
        const x = Math.max(0, Math.min(1, (root.t - root.introHold) / root.introGrow))
        return x * x * x * (x * (x * 6 - 15) + 10)
    }

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
        // The arrival drives the size, the disk's gain and its temperature:
        // a point of shadow that opens up and lights the scene as it grows.
        property real uGrow: root.grow
        property real uHoleRadius: 0.0008 + (0.125 - 0.0008) * root.grow
        property real uGain: 0.06 + (1.1 - 0.06) * root.grow
        property real uTemp: 2900 + (4200 - 2900) * root.grow
        property real uDiskInner: 1.8
        property real uDiskOuter: 8.0
        property real uIncl: 1.45
        property real uRoll: 0.35
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

    Component.onCompleted: {
        root.lastTick = Date.now()
        // Every show starts the arrival over, including a repeat of the same
        // scene (the loader keeps this item alive across a hide).
        root.cover.shownChanged.connect(function() {
            if (!root.cover.shown)
                return
            root.t = 0
            root.lastTick = Date.now()
        })
    }
}
