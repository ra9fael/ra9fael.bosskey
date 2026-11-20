// BlackholeScene.qml - a lensed accretion disk and stars spiraling into the event horizon.
import QtQuick

Item {
    id: root

    required property var cover
    required property int screenIndex

    readonly property real unit: Math.min(width, height)
    readonly property real cx: width / 2
    readonly property real cy: height / 2
    readonly property real holeRadius: unit * 0.105
    readonly property real diskRadius: unit * 0.39
    property var stars: []
    property real lastTick: 0
    property real pulse: 0

    function randomFor(seed) {
        let value = seed >>> 0
        return function() {
            value = (value * 1664525 + 1013904223) >>> 0
            return value / 4294967296
        }
    }

    function initStars() {
        const rand = randomFor(0x42484f4c + (screenIndex + 1) * 104729)
        const next = []
        for (let i = 0; i < 180; ++i) {
            const radius = holeRadius * 1.5 + rand() * (diskRadius * 2.1 - holeRadius * 1.5)
            next.push({
                angle: rand() * Math.PI * 2,
                radius: radius,
                speed: 10 + 60 * holeRadius / radius,
                size: 0.7 + rand() * 1.5
            })
        }
        stars = next
        lastTick = Date.now()
        disk.requestPaint()
    }

    Rectangle {
        anchors.fill: parent
        color: "#020207"
    }

    Canvas {
        id: disk
        anchors.fill: parent
        renderStrategy: Canvas.Immediate

        onPaint: {
            const ctx = getContext("2d")
            ctx.reset()
            ctx.fillStyle = "#020207"
            ctx.fillRect(0, 0, width, height)

            const cx = root.cx
            const cy = root.cy
            const r = root.holeRadius
            const outer = root.diskRadius

            // Sparse stars and inward-curving streaks.
            for (let i = 0; i < root.stars.length; ++i) {
                const star = root.stars[i]
                const x = cx + Math.cos(star.angle) * star.radius
                const y = cy + Math.sin(star.angle) * star.radius * 0.58
                const tailRadius = Math.min(star.radius + star.speed * 0.18, outer * 2.2)
                const tx = cx + Math.cos(star.angle - 0.045) * tailRadius
                const ty = cy + Math.sin(star.angle - 0.045) * tailRadius * 0.58
                const alpha = Math.min(0.85, (star.radius - r) / (outer * 1.7))
                ctx.strokeStyle = "rgba(170,205,255," + Math.max(0.08, alpha) + ")"
                ctx.lineWidth = star.size
                ctx.beginPath()
                ctx.moveTo(tx, ty)
                ctx.lineTo(x, y)
                ctx.stroke()
                ctx.fillStyle = "rgba(238,246,255," + Math.max(0.18, alpha) + ")"
                ctx.fillRect(x, y, star.size, star.size)
            }

            // Broad upper arc sits behind the event horizon.
            ctx.save()
            ctx.translate(cx, cy)
            ctx.scale(1, 0.52)
            for (let i = 0; i < 28; ++i) {
                const t = i / 27
                const radius = r * 1.05 + (outer - r * 1.05) * t
                const red = 255
                const green = Math.round(220 - 175 * t)
                const blue = Math.round(156 - 145 * t)
                ctx.strokeStyle = "rgba(" + red + "," + green + "," + blue + "," + (0.14 + 0.45 * (1 - t)) + ")"
                ctx.lineWidth = Math.max(2, unit * 0.055 * (1 - t * 0.7))
                ctx.beginPath()
                ctx.ellipse(0, 0, radius, radius, 0, Math.PI, Math.PI * 2)
                ctx.stroke()
            }
            ctx.restore()

            // Soft outer glow and the asymmetric hot side of the disk.
            const glow = ctx.createRadialGradient(cx, cy, r * 0.9, cx, cy, outer)
            glow.addColorStop(0, "rgba(255,210,132,0.42)")
            glow.addColorStop(0.42, "rgba(248,105,35,0.14)")
            glow.addColorStop(1, "rgba(0,0,0,0)")
            ctx.fillStyle = glow
            ctx.fillRect(cx - outer, cy - outer, outer * 2, outer * 2)

            // Event horizon and photon ring.
            const halo = ctx.createRadialGradient(cx, cy, r * 0.92, cx, cy, r * 1.24)
            halo.addColorStop(0, "rgba(0,0,0,1)")
            halo.addColorStop(0.72, "rgba(0,0,0,1)")
            halo.addColorStop(0.88, "rgba(255,247,225,0.98)")
            halo.addColorStop(1, "rgba(255,173,86,0.08)")
            ctx.fillStyle = halo
            ctx.beginPath()
            ctx.arc(cx, cy, r * 1.24, 0, Math.PI * 2)
            ctx.fill()
            ctx.fillStyle = "#000000"
            ctx.beginPath()
            ctx.arc(cx, cy, r * 0.99, 0, Math.PI * 2)
            ctx.fill()

            // Bright lower arc crosses in front of the event horizon.
            ctx.save()
            ctx.translate(cx, cy)
            ctx.scale(1, 0.52)
            for (let i = 0; i < 28; ++i) {
                const t = i / 27
                const radius = r * 1.05 + (outer - r * 1.05) * t
                const red = 255
                const green = Math.round(214 - 170 * t)
                const blue = Math.round(135 - 124 * t)
                ctx.strokeStyle = "rgba(" + red + "," + green + "," + blue + "," + (0.25 + 0.62 * (1 - t)) + ")"
                ctx.lineWidth = Math.max(2, unit * 0.06 * (1 - t * 0.7))
                ctx.beginPath()
                ctx.ellipse(0, 0, radius, radius, 0, 0, Math.PI)
                ctx.stroke()
            }
            ctx.restore()
        }
    }

    Timer {
        running: root.cover.shown
        interval: 66
        repeat: true
        onTriggered: {
            const now = Date.now()
            const dt = Math.min(0.1, (now - root.lastTick) / 1000)
            root.lastTick = now
            root.pulse += dt
            const next = root.stars.slice()
            for (let i = 0; i < next.length; ++i) {
                const star = next[i]
                star.radius -= star.speed * dt
                star.angle += dt * 0.015
                if (star.radius < root.holeRadius * 1.35) {
                    star.radius = root.diskRadius * (1.4 + Math.random() * 0.7)
                    star.angle = Math.random() * Math.PI * 2
                }
            }
            root.stars = next
            disk.requestPaint()
        }
    }

    onWidthChanged: initStars()
    onHeightChanged: initStars()
    Component.onCompleted: initStars()
}
