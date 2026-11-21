// BlackholeScene.qml - a Schwarzschild black hole on a transparent cover: the
// desktop behind is lensed around the hole, the shadow swallows it, and the
// accretion disk lights the scene. Every pixel's null geodesic is integrated in
// the fragment shader. Physics and porting notes: docs/blackhole-shader.md.
import QtQuick

Item {
    id: root

    required property var cover
    required property int screenIndex

    // Wall-clock seconds: paused while hidden and reset on every show.
    property real t: 0
    property real lastTick: 0

    // Bounded Brownian motion in normalized screen coordinates. The filtered
    // velocity keeps it wandering rather than jittering; the soft spring keeps
    // it near the centre of the captured desktop.
    property real walkX: 0
    property real walkY: 0
    property real velocityX: 0
    property real velocityY: 0

    readonly property real introHold: 0.8
    readonly property real seedDuration: 0.6
    readonly property real diskDuration: 3.5
    readonly property real holeDuration: 6.5
    readonly property real holeStart: introHold + diskDuration

    function smootherstep(value) {
        const x = Math.max(0, Math.min(1, value))
        return x * x * x * (x * (x * 6 - 15) + 10)
    }

    readonly property real seedProgress: smootherstep((t - introHold) / seedDuration)
    readonly property real diskProgress: smootherstep((t - introHold) / diskDuration)
    readonly property real holeProgress: smootherstep((t - holeStart) / holeDuration)

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
        property real uCenterX: root.walkX
        property real uCenterY: root.walkY
        property variant uDesktop: desktop

        // First a small seed appears, then the disk spreads to 18 r_s while the
        // shadow stays small. Only after the disk is established does the shadow
        // slowly grow; as it does, the disk returns to its steady-state radius.
        property real uHoleRadius: root.t < root.introHold + root.seedDuration
                                  ? 0.0008 + (0.02 - 0.0008) * root.seedProgress
                                  : 0.02 + (0.125 - 0.02) * root.holeProgress
        property real uDiskInner: 1.8 + 1.8 * root.diskProgress * (1 - root.holeProgress)
        property real uDiskOuter: root.t < root.holeStart
                                  ? 5.0 + (18.0 - 5.0) * root.diskProgress
                                  : 18.0 + (8.0 - 18.0) * root.holeProgress
        property real uGain: 0.06 + (1.1 - 0.06) * root.diskProgress
        property real uTemp: 2900 + (4200 - 2900) * root.diskProgress
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

    // 30 fps: the cover can hang for minutes; motion and disk rotation pause
    // with it. A fresh random walk starts from the centre on every show.
    Timer {
        running: root.cover.shown
        interval: 33
        repeat: true
        onTriggered: {
            const now = Date.now()
            const dt = Math.min(0.25, Math.max(0, (now - root.lastTick) / 1000))
            root.t += dt
            root.lastTick = now

            if (root.t <= root.introHold + root.seedDuration)
                return

            const noise = 0.10 * Math.sqrt(dt)
            let vx = (root.velocityX + (Math.random() - 0.5) * noise) * Math.exp(-0.8 * dt)
            let vy = (root.velocityY + (Math.random() - 0.5) * noise) * Math.exp(-0.8 * dt)
            const radius = Math.sqrt(root.walkX * root.walkX + root.walkY * root.walkY)
            const pull = Math.max(0, radius - 0.045) * 1.8
            vx -= root.walkX * pull * dt
            vy -= root.walkY * pull * dt
            vx = Math.max(-0.04, Math.min(0.04, vx))
            vy = Math.max(-0.04, Math.min(0.04, vy))
            root.velocityX = vx
            root.velocityY = vy
            root.walkX = Math.max(-0.08, Math.min(0.08, root.walkX + vx * dt))
            root.walkY = Math.max(-0.08, Math.min(0.08, root.walkY + vy * dt))
        }
    }

    Component.onCompleted: {
        root.lastTick = Date.now()
        root.cover.shownChanged.connect(function() {
            if (!root.cover.shown)
                return
            root.t = 0
            root.walkX = 0
            root.walkY = 0
            root.velocityX = 0
            root.velocityY = 0
            root.lastTick = Date.now()
        })
    }
}
