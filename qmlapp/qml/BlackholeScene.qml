// BlackholeScene.qml - a Schwarzschild black hole over a dark sky: the dust and
// stars behind it are lensed around the hole, the shadow swallows them, and the
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
    // it near the centre of the frame.
    property real walkX: 0
    property real walkY: 0
    property real velocityX: 0
    property real velocityY: 0

    // The dark base tone, sky detail level, and corner darkening. The opaque sky
    // keeps desktop content out of the scene entirely.
    readonly property color skyTone: "#02040a"
    readonly property real skyGain: 0.35
    readonly property real vignette: 0.55

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

    // Opaque from the first frame, including while the shader initializes.
    Rectangle {
        anchors.fill: parent
        color: root.skyTone
    }

    ShaderEffect {
        anchors.fill: parent

        property real uAspect: Math.max(0.0001, width / height)
        property real uTime: root.t
        property real uCenterX: root.walkX
        property real uCenterY: root.walkY
        property color uSky: root.skyTone
        property real uBgGain: root.skyGain
        property real uVignette: root.vignette
        // The glow only starts with the disk: nothing is burning at the seed.
        property real uBloom: root.diskProgress

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
        property real uGain: 0.06 + (0.75 - 0.06) * root.diskProgress
        property real uTemp: 2900 + (4200 - 2900) * root.diskProgress
        property real uIncl: 1.45
        property real uRoll: 0.35
        property real uExposure: 1.15
        property real uSpeed: 5.0
        property real uWind: 7.0
        property real uContrast: 2.0
        property real uDoppler: 0.6
        property real uBeam: 2.2
        property real uOpacity: 0.95
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
