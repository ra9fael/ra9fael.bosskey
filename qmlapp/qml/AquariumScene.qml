// AquariumScene.qml - the ASCII fish tank, ported from POWER-PILL/ASCII-Aquarium
// v2.39 (MIT), the ESP32 firmware that drew it on a 320x240 CYD LCD.
//
// The simulation keeps the firmware's world: 320x240 units, its constants and
// its order of updates. One tank covers the window while preserving its aspect
// ratio; the longer dimension is cropped instead of leaving an unpainted side.
// Glyphs are ASCII only, in a Windows system monospace font.
import QtQuick
import "Aquarium.js" as Tank

Rectangle {
    id: root

    // The aquarium owns an opaque surface. Pixels outside the Canvas remain
    // black instead of exposing a previously composited desktop frame.
    color: "#000000"

    required property var cover
    required property int screenIndex
    property bool spanScreens: false
    property real spanScale: 1
    property int spanMonitorCount: 1

    // ---- the world ----------------------------------------------------------
    readonly property int worldW: spanScreens ? Math.max(320, Math.ceil(width / p)) : 320
    readonly property int worldH: spanScreens ? Math.max(240, Math.ceil(height / p)) : 240 // SCREEN_H
    readonly property int seaLevelY: worldH - 36 // SEA_LEVEL_Y
    readonly property int tickMs: 50            // the firmware's 20 fps

    // The firmware's pools and limits.
    readonly property int populationScale: spanScreens ? Math.max(1, spanMonitorCount) : 1
    readonly property int poolFish: 48 * populationScale // MAX_FISH_POOL per display
    readonly property int maxFlakes: 16 * populationScale
    readonly property int maxBubbles: 50 * populationScale
    readonly property int maxAutoFeed: 9 * populationScale
    readonly property int cthulhuPellets: 60 * populationScale // CTHULHU_FEED_PELLET_THRESHOLD

    // Tank settings: the firmware's fish, bubbles and the pace of its clock.
    // The cover controller has clamped fish and bubbles; 1 is ordinary pace.
    readonly property var tank: cover.tank
    property int fishTarget: tank.fish * populationScale
    property int bubbleTarget: tank.bubbles * populationScale
    property real speed: Math.max(0.5, Math.min(2, tank.speed || 1))

    // ---- the screen ---------------------------------------------------------
    //
    // Scale until the 4:3 tank covers both dimensions. Fractional scaling is
    // intentional here: full-screen windows are not restricted to integer LCD
    // pixels, and cropping the long dimension avoids a black strip at the side.
    readonly property real p: spanScreens ? spanScale : Math.max(width / worldW, height / worldH)
    readonly property real boardW: worldW * p
    readonly property real boardH: worldH * p
    readonly property int cols: 1
    readonly property int rows: 1
    readonly property int originX: Math.floor((width - boardW) / 2)
    readonly property int originY: Math.floor((height - boardH) / 2)

    // Single-screen Tab cycles through the aquarium overlays. In a multi-screen
    // --all cover, BossCover routes Tab to the native layout cycler instead.
    property int effectMode: 0

    function cycleEffect() {
        effectMode = (effectMode + 1) % 3
        paint()
    }

    // One tank's state --------------------------------------------------------
    //
    // One continuous world owns the clock and all simulated creatures. The
    // population scales with covered displays while each creature keeps the
    // firmware's original movement speed.

    // A tank is a plain object; twelve fields per fish, thirty per visitor.
    property var nextSeed: 0x41515541 + (screenIndex + 1) * 7919
    function seeded() {
        let value = nextSeed >>> 0
        return function() {
            value = (Math.imul(value, 1664525) + 1013904223) >>> 0
            return value / 4294967296
        }
    }

    function rand(r, lo, hi) {
        return lo + r() * (hi - lo)
    }

    function irand(r, lo, hi) {
        return lo + Math.floor(r() * (hi - lo))
    }

    function clamp(v, lo, hi) {
        return v < lo ? lo : (v > hi ? hi : v)
    }

    // A seeded tank: the same one on every show, a different one per board.
    function makeTank(index) {
        nextSeed = (0x41515541 + (screenIndex + 1) * 7919) ^ Math.imul(index + 1, 2654435761)
        const r = seeded()
        const k = {
            r: r, t: 0,
            fish: [], flakes: [], bubbles: [], seaweed: [],
            octopus: visitor(8), seahorse: visitor(24), snail: visitor(6),
            jellyfish: visitor(14),
            pellets: 0,
            nextFeed: rand(r, 8, 14) / populationScale,
            sprinkle: false, sprinkleToRight: true, sprinkleLeft: 0, sprinkleNext: 0, sprinkleEvery: 0.2
        }
        for (let i = 0; i < poolFish; ++i)
            k.fish.push(makeFish(r))
        spreadInitialFishLayout(k)
        for (let i = 0; i < maxFlakes; ++i)
            k.flakes.push({ active: false, x: 0, y: 0, vy: 30, color: 0 })
        for (let i = 0; i < maxBubbles; ++i) {
            const b = { active: false, x: 0, y: 0, baseX: 0, vy: 14, phase: 0, sway: 4, color: 0 }
            k.bubbles.push(b)
        }
        applyBubblePopulation(k, true)
        buildSeaweed(k)
        return k
    }

    // ---- the fish -----------------------------------------------------------
    //
    // The firmware's Fish: a species, a depth, a velocity that steering nudges,
    // and a horizontal wrap.

    function makeFish(r) {
        return { active: false, type: 0, x: 0, y: 0, vx: 1, vy: 0, speed: 20, phase: 0,
                 wander: 1, width: 24, color: 0, depth: 1, render: 0 }
    }

    // activateFish: a species, one in five a rarer hue, a depth, a lane.
    function activateFish(k, f) {
        const r = k.r
        f.active = true
        f.type = irand(r, 0, Tank.species.length)
        f.width = Tank.advance(Tank.species[f.type].right)
        f.color = Tank.species[f.type].color
        if (irand(r, 0, 100) < 20)
            f.color = Tank.altColors[irand(r, 0, Tank.altColors.length)]
        f.depth = Tank.randomDepth(r)
        f.render = Tank.scaleRgb565(f.color, f.depth)
        f.x = rand(r, -42, worldW + 12) // side entry
        f.y = rand(r, 20, seaLevelY - 10)
        f.vx = rand(r, -1, 1)
        f.vy = rand(r, -0.5, 0.5)
        f.speed = rand(r, 14, 30)
        f.phase = rand(r, 0, 6.28318)
        f.wander = rand(r, 0.4, 1.3)
    }

    // applyFishPopulation: the first fishTarget slots swim, the rest wait.
    function applyFishPopulation(k) {
        for (let i = 0; i < poolFish; ++i) {
            const f = k.fish[i]
            if (i < fishTarget) {
                if (!f.active)
                    activateFish(k, f)
            } else if (f.active) {
                f.active = false
            }
        }
    }

    // spreadInitialFishLayout: a new tank starts with no two fish on top of each
    // other, then jitters them until nothing is crowded.
    function spreadInitialFishLayout(k) {
        const r = k.r
        const minGapX = 52 * 0.92
        const minGapY = 20 * 1.05
        const cx0 = f => f.x + f.width * 0.5
        for (let i = 0; i < fishTarget; ++i) {
            const f = k.fish[i]
            activateFish(k, f)
            f.y = rand(r, 18, seaLevelY - 18)
            for (let tries = 0; tries < 20; ++tries) {
                let clear = true
                for (let j = 0; j < i; ++j) {
                    const o = k.fish[j]
                    if (Math.abs(cx0(o) - cx0(f)) < minGapX && Math.abs(o.y - f.y) < minGapY) {
                        clear = false
                        break
                    }
                }
                if (clear)
                    break
                f.x = rand(r, -42, worldW + 12)
                f.y = rand(r, 18, seaLevelY - 18)
            }
        }
        // The rest of the pool is seated around the last fish, as the firmware
        // does, off the bottom of the tank at the narrowest.
        const last = k.fish[fishTarget - 1] || k.fish[0]
        for (let i = fishTarget; i < poolFish; ++i) {
            const f = k.fish[i]
            if (!f.active)
                activateFish(k, f)
            f.x = last.x + rand(r, -8, 8)
            f.y = clamp(last.y + rand(r, -8, 8), 18, seaLevelY - 18)
        }
    }

    function centerOf(f) {
        return { x: f.x + f.width * 0.5, y: f.y + 7 } // FISH_CENTER_Y_OFFSET
    }

    // updateFish: wander, school with the same species, repel, chase the
    // nearest flake, steer clear of the visitors, then swim at speed, then be
    // pushed out of anything it has ended up inside. Horizontal edges wrap.
    function updateFish(k, dt) {
        const fish = k.fish
        const centers = k.centers || (k.centers = [])
        for (let i = 0; i < poolFish; ++i) {
            if (fish[i].active)
                centers[i] = centerOf(fish[i])
        }

        for (let i = 0; i < poolFish; ++i) {
            const f = fish[i]
            if (!f.active)
                continue
            f.vx += Math.cos(f.phase + k.t * 0.9) * 0.45 * f.wander * dt
            f.vy += Math.sin(f.phase * 1.7 + k.t * 0.7) * 0.22 * dt

            let avgX = 0, avgY = 0, cx = 0, cy = 0, near = 0
            let repelX = 0, repelY = 0, repel = 0
            const c = centers[i]
            for (let j = 0; j < poolFish; ++j) {
                if (i === j || !fish[j].active)
                    continue
                const o = fish[j]
                if (o.type === f.type) {
                    const dx = o.x - f.x
                    const dy = o.y - f.y
                    if (dx * dx + dy * dy < 3600) { // within ~60 units
                        avgX += o.vx
                        avgY += o.vy
                        cx += o.x
                        cy += o.y
                        ++near
                    }
                }
                let sdx = centers[j].x - c.x
                const sdy = centers[j].y - c.y
                if (sdx > worldW * 0.5)
                    sdx -= worldW
                if (sdx < -worldW * 0.5)
                    sdx += worldW
                const sx = sdx / 52 // FISH_AVOID_RADIUS_X
                const sy = sdy / 20 // FISH_AVOID_RADIUS_Y
                const s2 = sx * sx + sy * sy
                if (s2 > 0.0001 && s2 < 1) {
                    const d = Math.sqrt(sdx * sdx + sdy * sdy) + 0.0001
                    const push = (1 - s2) * (1 - s2)
                    repelX -= sdx / d * push
                    repelY -= sdy / d * push
                    ++repel
                }
            }
            if (near > 0) {
                f.vx += (avgX / near - f.vx) * 0.45 * dt
                f.vy += (avgY / near - f.vy) * 0.25 * dt
                f.vx += (cx / near - f.x) * 0.0018
                f.vy += (cy / near - f.y) * 0.0012
            }

            const fi = closestFlake(k, f, 140)
            if (fi >= 0) {
                const flake = k.flakes[fi]
                const dx = flake.x - f.x
                const dy = flake.y - f.y
                const d = Math.sqrt(dx * dx + dy * dy) + 0.0001
                f.vx += dx / d * 0.95 * dt
                f.vy += dy / d * 0.95 * dt
                if (d < 8)
                    flake.active = false // eaten
            }

            steer(k, f, c, k.octopus, k.octopus.cthulhu ? 76 * 1.55 : 76,
                  k.octopus.cthulhu ? 34 * 1.55 : 34, k.octopus.cthulhu ? 8 * 1.85 : 8, dt)
            steer(k, f, c, k.seahorse, 58, 38, 6, dt)
            steer(k, f, c, k.snail, 30, 18, 4, dt)
            steer(k, f, c, k.jellyfish, 42, 38, 4.6, dt)
            steer(k, f, c, k.squid, 50, 24, 5.2, dt)

            if (repel > 0) {
                f.vx += repelX / repel * 4.2 * dt // FISH_AVOID_STRENGTH
                f.vy += repelY / repel * 4.2 * dt
            }
            if (f.y < 18)
                f.vy += 0.8 * dt
            if (f.y > seaLevelY - 8)
                f.vy -= 0.8 * dt

            let mag = Math.sqrt(f.vx * f.vx + f.vy * f.vy)
            if (mag < 0.0001) {
                f.vx = 1
                f.vy = 0
                mag = 1
            }
            f.vx /= mag
            f.vy /= mag
            const speed = f.speed + Math.sin(k.t * 3.2 + f.phase) * 4
            f.x += f.vx * speed * dt
            f.y += f.vy * speed * dt

            keepOutside(k, f, k.octopus, k.octopus.cthulhu ? 46 * 1.45 : 46,
                        k.octopus.cthulhu ? 22 * 1.45 : 22, 0.55)
            keepOutside(k, f, k.seahorse, 40, 26, 0.45)
            keepOutside(k, f, k.snail, 20, 12, 0.45)
            keepOutside(k, f, k.jellyfish, 30, 24, 0.4)
            keepOutside(k, f, k.squid, 34, 16, 0.4)

            const pad = f.width + 10
            if (f.x > worldW + pad) {
                f.x = -pad
                f.depth = Tank.randomDepth(k.r)
                f.render = Tank.scaleRgb565(f.color, f.depth)
            }
            if (f.x < -pad) {
                f.x = worldW + pad
                f.depth = Tank.randomDepth(k.r)
                f.render = Tank.scaleRgb565(f.color, f.depth)
            }
            f.y = clamp(f.y, 14, seaLevelY - 6)
        }
    }

    // steerFishAwayFrom*: a soft radial push out of a visitor's ellipse.
    function steer(k, f, c, v, radiusX, radiusY, strength, dt) {
        if (!v || !v.active)
            return
        const dx = c.x - v.x
        const dy = c.y - (v.y + v.centerY)
        const sx = dx / radiusX
        const sy = dy / radiusY
        const s2 = sx * sx + sy * sy
        if (s2 <= 0.0001 || s2 >= 1)
            return
        const d = Math.sqrt(dx * dx + dy * dy) + 0.0001
        const push = (1 - s2) * (1 - s2)
        f.vx += dx / d * push * strength * dt
        f.vy += dy / d * push * strength * dt
    }

    // keepFishOutside*: a fish inside a visitor's clear ellipse is moved out
    // onto it, along its own heading when it sits exactly at the centre.
    function keepOutside(k, f, v, radiusX, radiusY, amount) {
        if (!v || !v.active)
            return
        const cx = f.x + f.width * 0.5
        const cy = f.y + 7
        let dx = cx - v.x
        let dy = cy - (v.y + v.centerY)
        let s2 = (dx / radiusX) * (dx / radiusX) + (dy / radiusY) * (dy / radiusY)
        if (s2 >= 1)
            return
        if (s2 <= 0.0001) {
            dx = f.vx >= 0 ? 1 : -1
            dy = f.vy >= 0 ? 0.35 : -0.35
            s2 = (dx / radiusX) * (dx / radiusX) + (dy / radiusY) * (dy / radiusY)
        }
        const scale = 1 / Math.sqrt(s2)
        f.x += (v.x + dx * scale - cx) * amount
        f.y += (v.y + v.centerY + dy * scale - cy) * amount
    }

    function closestFlake(k, f, maxDist) {
        let best = -1
        let bestD2 = maxDist * maxDist
        for (let i = 0; i < maxFlakes; ++i) {
            const flake = k.flakes[i]
            if (!flake.active)
                continue
            const d2 = (flake.x - f.x) * (flake.x - f.x) + (flake.y - f.y) * (flake.y - f.y)
            if (d2 < bestD2) {
                bestD2 = d2
                best = i
            }
        }
        return best
    }

    // ---- flakes, bubbles and the feed ---------------------------------------

    function spawnFlake(k, x, y) {
        for (let i = 0; i < maxFlakes; ++i) {
            const flake = k.flakes[i]
            if (flake.active)
                continue
            flake.active = true
            flake.x = x
            flake.y = y
            flake.vy = rand(k.r, 22, 48)
            flake.color = Tank.randomFoodColor(k.r)
            if (k.pellets < cthulhuPellets)
                ++k.pellets
            return
        }
    }

    function updateFlakes(k, dt) {
        for (let i = 0; i < maxFlakes; ++i) {
            const flake = k.flakes[i]
            if (!flake.active)
                continue
            flake.y += flake.vy * dt
            flake.x += Math.sin(k.t * 1.2 + i) * 8 * dt
            if (flake.y > seaLevelY)
                flake.active = false
        }
    }

    function resetBubble(k, b, spreadOut) {
        const r = k.r
        b.active = true
        b.baseX = rand(r, 8, worldW - 8)
        b.x = b.baseX
        b.y = spreadOut ? rand(r, 4, worldH + 48) : rand(r, worldH - 4, worldH + 48)
        b.vy = rand(r, 12, 28)
        b.phase = rand(r, 0, 6.28318)
        b.sway = rand(r, 2, 7)
        b.color = Tank.randomBubbleColor(r)
    }

    function applyBubblePopulation(k, spreadNew) {
        for (let i = 0; i < maxBubbles; ++i) {
            const b = k.bubbles[i]
            if (i < bubbleTarget) {
                if (!b.active)
                    resetBubble(k, b, spreadNew)
            } else if (b.active) {
                b.active = false
            }
        }
    }

    function updateBubbles(k, dt) {
        for (let i = 0; i < maxBubbles; ++i) {
            const b = k.bubbles[i]
            if (!b.active)
                continue
            b.y -= b.vy * dt
            b.x = b.baseX + Math.sin(k.t * 1.8 + b.phase) * b.sway
            if (b.y < -10)
                resetBubble(k, b, false)
        }
    }

    // serviceAutoFeed: a sprinkle every autoFeedIntervalMs: nine flakes swept
    // across the tank from one side over 1.2 to 2.8 seconds. The firmware's
    // frequency setting is at its default here: a sprinkle every 30-70 s.
    function serviceAutoFeed(k) {
        if (k.sprinkle) {
            while (k.sprinkle && k.t >= k.sprinkleNext) {
                let progress = k.sprinkleLeft / (maxAutoFeed - 1)
                if (!k.sprinkleToRight)
                    progress = 1 - progress
                const x = 16 + progress * (worldW - 32) + rand(k.r, -5, 5)
                const y = rand(k.r, 8, 22)
                spawnFlake(k, clamp(x, 10, worldW - 10), y)
                ++k.sprinkleLeft
                if (k.sprinkleLeft >= maxAutoFeed) {
                    k.sprinkle = false
                } else {
                    k.sprinkleNext += k.sprinkleEvery
                    if (k.sprinkleNext < k.t)
                        k.sprinkleNext = k.t + k.sprinkleEvery
                }
            }
        }
        if (k.t >= k.nextFeed) {
            k.sprinkle = true
            k.sprinkleToRight = k.r() < 0.5
            k.sprinkleLeft = 0
            k.sprinkleNext = k.t
            k.sprinkleEvery = Math.max(0.12, rand(k.r, 1.2, 2.8) / (maxAutoFeed - 1))
            k.nextFeed = k.t + rand(k.r, 30, 70)
        }
    }

    // ---- the visitors -------------------------------------------------------
    //
    // Each one is spawned off one edge, swims across on its own clock and goes
    // inactive at the far edge, as the firmware's visitors did. The squid is
    // the exception: the firmware's SQUID_ENABLED is off, so it stays inactive.

    function visitor(centerY) {
        return { active: false, cthulhu: false, facingRight: true, x: 0, y: 0, baseY: 0,
                 vx: 1, phase: 0, finPhase: 0, colorPhase: 0, nextSpawn: 0, centerY: centerY }
    }

    // spawnOctopus: a fraction of them the deep one; bobbing at 0.45 Hz.
    function updateOctopus(k, dt) {
        const o = k.octopus
        if (!o.active) {
            if (k.t >= o.nextSpawn) {
                const fromLeft = k.r() < 0.5
                o.active = true
                if (k.pellets >= cthulhuPellets || k.r() < 0.02) {
                    o.cthulhu = true
                    k.pellets = 0
                } else {
                    o.cthulhu = false
                }
                o.vx = fromLeft ? rand(k.r, 4.5, 8) : -rand(k.r, 4.5, 8)
                o.x = fromLeft ? -42 : worldW + 42
                o.baseY = rand(k.r, 36, seaLevelY - 48)
                o.y = o.baseY
                o.phase = rand(k.r, 0, 6.28318)
                o.colorPhase = rand(k.r, 0, 6.28318)
                o.nextSpawn = k.t + rand(k.r, 40, 120) / populationScale
            }
            return
        }
        o.x += o.vx * dt
        o.y = o.baseY + Math.sin(k.t * 0.45 + o.phase) * 6
        if ((o.vx > 0 && o.x > worldW + 42) || (o.vx < 0 && o.x < -42))
            o.active = false
    }

    // spawnSeahorse: a pulse in its speed, two bobs in its height.
    function updateSeahorse(k, dt) {
        const s = k.seahorse
        if (!s.active) {
            if (k.t >= s.nextSpawn) {
                const fromLeft = k.r() < 0.5
                s.active = true
                s.facingRight = fromLeft
                s.vx = (fromLeft ? rand(k.r, 1.6, 2.9) : -rand(k.r, 1.6, 2.9)) * 1.35
                s.x = fromLeft ? -48 : worldW + 48
                s.baseY = rand(k.r, 34, seaLevelY - 56)
                s.y = s.baseY
                s.phase = rand(k.r, 0, 6.28318)
                s.finPhase = rand(k.r, 0, 6.28318)
                s.nextSpawn = k.t + rand(k.r, 60, 160) / populationScale
            }
            return
        }
        const pulse = 1 + Math.sin(k.t * 0.55 + s.phase) * 0.18
        s.x += s.vx * pulse * dt
        s.y = s.baseY + Math.sin(k.t * 0.82 + s.phase) * 4.5
            + Math.sin(k.t * 2.15 + s.phase * 1.7) * 0.9
        if ((s.vx > 0 && s.x > worldW + 48) || (s.vx < 0 && s.x < -48))
            s.active = false
    }

    // spawnSnail: along the floor, scooting.
    function updateSnail(k, dt) {
        const s = k.snail
        if (!s.active) {
            if (k.t >= s.nextSpawn) {
                const fromLeft = k.r() < 0.5
                s.active = true
                s.facingRight = fromLeft
                s.vx = fromLeft ? rand(k.r, 2.2, 3.7) : -rand(k.r, 2.2, 3.7)
                s.x = fromLeft ? -24 : worldW + 24
                s.baseY = worldH - 15
                s.y = s.baseY
                s.phase = rand(k.r, 0, 6.28318)
                s.nextSpawn = k.t + rand(k.r, 90, 200) / populationScale
            }
            return
        }
        s.x += s.vx * (1 + Math.sin(k.t * 2.2 + s.phase) * 0.18) * dt
        s.y = worldH - 15
        if ((s.vx > 0 && s.x > worldW + 24) || (s.vx < 0 && s.x < -24))
            s.active = false
    }

    // spawnJellyfish: in at mid-depth, pulsing upward, leaving at the top.
    function updateJellyfish(k, dt) {
        const j = k.jellyfish
        if (!j.active) {
            if (k.t >= j.nextSpawn) {
                const fromLeft = k.r() < 0.5
                j.active = true
                j.x = fromLeft ? -34 : worldW + 34
                j.baseY = seaLevelY * 0.48
                j.y = j.baseY
                j.vx = fromLeft ? rand(k.r, 2.2, 4) : -rand(k.r, 2.2, 4)
                j.phase = rand(k.r, 0, 6.28318)
                j.nextSpawn = k.t + rand(k.r, 70, 160) / populationScale
            }
            return
        }
        const wave = Math.sin(k.t * 1.55 + j.phase)
        const pulse = wave > 0 ? wave * wave : 0
        j.x += (j.vx + Math.sin(k.t * 0.38 + j.phase) * 2) * dt
        j.baseY -= pulse * 3.2 * dt
        j.baseY += Math.sin(k.t * 0.22 + j.phase) * 0.42 * dt
        j.y = clamp(j.baseY + Math.sin(k.t * 0.72 + j.phase) * 5 - pulse * 2.5, 24, seaLevelY - 34)
        if (j.x < -34 || j.x > worldW + 34 || j.baseY < 10)
            j.active = false
    }

    // keepVisitorsSeparated: the seahorse gives way to the octopus, and the
    // octopus is nudged the other way.
    function keepVisitorsSeparated(k) {
        const o = k.octopus
        const s = k.seahorse
        if (!o.active || !s.active)
            return
        const ox = o.x
        const oy = o.y + o.centerY
        const hx = s.x + 15
        const hy = s.y + s.centerY
        let dx = hx - ox
        let dy = hy - oy
        let s2 = (dx / 64) * (dx / 64) + (dy / 40) * (dy / 40)
        if (s2 >= 1)
            return
        if (s2 <= 0.0001) {
            dx = s.vx >= o.vx ? 1 : -1
            dy = 0.35
            s2 = (dx / 64) * (dx / 64) + (dy / 40) * (dy / 40)
        }
        const scale = 1 / Math.sqrt(s2)
        const px = (ox + dx * scale - hx) * 0.18
        const py = (oy + dy * scale - hy) * 0.22
        s.x += px
        s.baseY = clamp(s.baseY + py, 24, seaLevelY - 54)
        s.y += py
        o.x -= px * 0.55
        o.baseY = clamp(o.baseY - py * 0.55, 28, seaLevelY - 44)
        o.y -= py * 0.55
    }

    // ---- the seaweed --------------------------------------------------------
    //
    // Twelve roots at the firmware's DEFAULT_SWAY (1.10) and
    // DEFAULT_SEAWEED_LENGTH (1.35).
    function buildSeaweed(k) {
        k.seaweed = []
        const roots = 12 * Math.max(1, Math.ceil(worldW / 320))
        for (let i = 0; i < roots; ++i) {
            const height = clamp(32 * 1.35 * (1 + 0.35 * Math.sin(i * 2.173 + 0.61)), 18, 72)
            k.seaweed.push({ x: 10 + i * (worldW - 20) / Math.max(1, roots - 1),
                             amp: 5 + (i % 4) * 2, height: height })
        }
    }

    // seaweedPoint: the blade's bend, travel and ripple, and its root's sway.
    function seaweedPoint(k, us, i) {
        const blade = k.seaweed[i]
        const sway = Math.sin(k.t * (0.8 + 0.09 * i) * 1.10 + i * 0.7) * blade.amp
        const body = Math.sin(k.t * (1.05 + i * 0.025) * 1.10 - us * 5.1 + i * 0.72)
        const ripple = Math.sin(k.t * 0.72 * 1.10 + us * 9.0 + i * 1.31)
        return { x: blade.x + sway * us * (0.20 + us * 0.80)
                     + body * (1.5 + blade.height * 0.055) * us * us + ripple * 1.2 * us,
                 y: (worldH - 2) - blade.height * us,
                 sway: sway }
    }

    // ---- the clock ----------------------------------------------------------

    property var tanks: []
    property bool built: false

    function resizeTanks() {
        const wanted = cols * rows
        if (tanks.length !== wanted) {
            tanks = []
            for (let i = 0; i < wanted; ++i)
                tanks.push(makeTank(i))
        }
        built = true
    }

    // A show starts a fresh tank on every board, from the same seed each time.
    function reset() {
        tanks = []
        resizeTanks()
        lastTick = Date.now()
        paint()
    }

    property real lastTick: 0

    function step() {
        const now = Date.now()
        const dt = Math.min(0.25, Math.max(0, (now - lastTick) / 1000)) * speed
        lastTick = now
        for (let i = 0; i < tanks.length; ++i)
            stepTank(tanks[i], dt)
        paint()
    }

    function stepTank(k, dt) {
        k.t += dt
        applyFishPopulation(k)
        applyBubblePopulation(k, false)
        updateBubbles(k, dt)
        updateFlakes(k, dt)
        serviceAutoFeed(k)
        updateFish(k, dt)
        updateOctopus(k, dt)
        updateSeahorse(k, dt)
        updateSnail(k, dt)
        updateJellyfish(k, dt)
        keepVisitorsSeparated(k)
    }

    // ---- drawing ------------------------------------------------------------
    //
    // One paint of the window: for each tank its water, then its layers in the
    // firmware's order. Every tank pixel is a p by p square of the window; the
    // seaweed keeps its one-pixel lines by drawing at p, and glyphs are placed
    // on the tank grid and sized by p.

    // paint: a new window, a new size, or the scene's own onPaint. The window
    // clears its canvas each frame, so the tank is drawn again after the
    // frame; a window whose canvas is not ready yet paints itself when it is.
    function paint() {
        if (drawing)
            pending = true
        else
            requestDraw()
    }

    property bool drawing: false
    property bool pending: false

    function requestDraw() {
        if (!canvas.available || width < 1 || height < 1)
            return
        pending = false
        drawing = true
        canvas.requestPaint()
        Qt.callLater(function() {
            drawing = false
            if (pending)
                requestDraw()
        })
    }

    function draw() {
        if (!canvas.available || width < 1 || height < 1)
            return
        if (!built || tanks.length !== cols * rows)
            resizeTanks()
        const ctx = canvas.getContext("2d")
        ctx.reset()
        ctx.globalCompositeOperation = "source-over"
        ctx.globalAlpha = 1
        ctx.textBaseline = "alphabetic"
        const u = p
        const canvasWidth = canvas.width
        const canvasHeight = canvas.height
        // Keep this foreground layer transparent so the physical-resolution
        // background Canvas remains visible beneath the moving creatures.
        ctx.clearRect(0, 0, width, height)
        // Each board: its water and its tank on the board's grid, p by p. The
        // window shows the part of the board that falls inside it.
        for (let c = 0; c < cols; ++c) {
            for (let r = 0; r < rows; ++r) {
                const ox = c * boardW
                const oy = r * boardH
                const ex = Math.min(boardW, canvasWidth - ox)
                const ey = Math.min(boardH, canvasHeight - oy)
                if (ex <= 0 || ey <= 0)
                    continue
                ctx.save()
                ctx.beginPath()
                ctx.rect(ox, oy, ex, ey)
                ctx.clip()
                drawTank(ctx, tanks[c * rows + r], ox, oy, u)
                ctx.restore()
            }
        }
    }

    function drawTank(ctx, k, ox, oy, u) {
        // Seaweed first: it is behind everything.
        for (let i = 0; i < k.seaweed.length; ++i)
            drawSeaweed(ctx, k, i, ox, oy, u)
        for (let i = 0; i < maxBubbles; ++i)
            if (k.bubbles[i].active)
                glyphCenter(ctx, ox, oy, u, "o", k.bubbles[i].x, k.bubbles[i].y, k.bubbles[i].color)
        for (let i = 0; i < maxFlakes; ++i)
            if (k.flakes[i].active)
                glyphCenter(ctx, ox, oy, u, "*", k.flakes[i].x, k.flakes[i].y, k.flakes[i].color)
        for (let i = 0; i < poolFish; ++i)
            if (k.fish[i].active)
                drawFish(ctx, k, k.fish[i], ox, oy, u)
        drawOctopus(ctx, k, ox, oy, u)
        drawSeahorse(ctx, k, ox, oy, u)
        drawSnail(ctx, k, ox, oy, u)
        drawJellyfish(ctx, k, ox, oy, u)
        drawEffect(ctx, k, ox, oy, u)
        if (effectMode === 2)
            drawClock(ctx, ox, oy, u)
    }

    function drawEffect(ctx, k, ox, oy, u) {
        if (effectMode === 1)
            drawNeonTitle(ctx, k, ox, oy, u)
    }

    // The source is a 320x240 title frame. Only its title band is drawn so the
    // transparent canvas margins cannot make the effect dominate the tank.
    function drawNeonTitle(ctx, k, ox, oy, u) {
        if (titleImage.status !== Image.Ready)
            return
        const sourceX = 10
        const sourceY = 67
        const sourceW = 310
        const sourceH = 111
        const titleW = 160
        const titleH = 57
        const titleX = (worldW - titleW) * 0.5
        const titleY = 34
        const pulse = 0.78 + 0.12 * Math.sin(k.t * 2.1)

        ctx.save()
        ctx.globalAlpha = pulse
        ctx.drawImage(titleImage, sourceX, sourceY, sourceW, sourceH,
                       ox + titleX * u, oy + titleY * u, titleW * u, titleH * u)
        ctx.restore()
    }

    function twoDigits(value) {
        return value < 10 ? "0" + value : String(value)
    }

    function drawClock(ctx, ox, oy, u) {
        const now = new Date()
        const time = twoDigits(now.getHours()) + ":" + twoDigits(now.getMinutes())
                    + ":" + twoDigits(now.getSeconds())
        const rows = 5
        const gap = 1
        let columns = 0
        for (let i = 0; i < time.length; ++i)
            columns += Tank.asciiClockWidth(time.charAt(i)) + (i ? gap : 0)
        const startX = Math.max(0, Math.floor((worldW - columns * 6) * 0.5))
        const startY = 82
        const shadow = Tank.css565(Tank.rgb565(0, 4, 22))
        const color = Tank.css565(Tank.rgb565(0, 20, 95))

        for (let row = 0; row < rows; ++row) {
            let x = startX
            for (let i = 0; i < time.length; ++i) {
                const glyph = Tank.asciiClockGlyphs[time.charAt(i)] || Tank.asciiClockGlyphs[" "]
                const line = glyph[row] || ""
                for (let col = 0; col < line.length; ++col) {
                    if (line.charAt(col) !== " ") {
                        glyphAt(ctx, x + col * 6 + 1, startY + row * 10 + 1,
                                line.charAt(col), u, ox, oy, shadow)
                        glyphAt(ctx, x + col * 6, startY + row * 10,
                                line.charAt(col), u, ox, oy, color)
                    }
                }
                x += (Tank.asciiClockWidth(time.charAt(i)) + gap) * 6
            }
        }
    }

    Image {
        id: titleImage
        source: "AquariumTitle.png"
        sourceSize: Qt.size(320, 240)
        visible: false
        onStatusChanged: if (status === Image.Ready) root.paint()
    }

    // The firmware's font, at the tank's own cell: 6 by 8 pixels per glyph.
    readonly property string fontFamily: "Consolas, 'Courier New', monospace"

    // drawChar, TL_DATUM: one glyph whose cell starts at (x, y) on the tank's
    // grid. The cell is the glyph's box, so a glyph never reaches its
    // neighbour's; Canvas clips to the box it is given.
    function glyphAt(ctx, x, y, text, u, ox, oy, color) {
        ctx.fillStyle = color
        ctx.font = Math.round(8 * u) + "px " + fontFamily
        ctx.fillText(text, ox + Math.round(x * u), oy + Math.round((y + 7) * u), 6 * u)
    }

    // drawString, MC_DATUM: a string centred on a tank point, over as many
    // cells as it has characters.
    function glyphCenter(ctx, ox, oy, u, text, x, y, color) {
        ctx.fillStyle = typeof color === "string" ? color : Tank.css(color)
        ctx.font = Math.round(8 * u) + "px " + fontFamily
        ctx.textAlign = "center"
        ctx.fillText(text, ox + Math.round(x * u), oy + Math.round((y + 3) * u), text.length * 6 * u)
        ctx.textAlign = "start"
    }

    // drawFish: every glyph its own cell, the swim wave travelling along the
    // body one FISH_SWIM_WAVE_SPACING (0.85 rad) at a time.
    function drawFish(ctx, k, f, ox, oy, u) {
        const right = f.vx >= 0
        const text = right ? Tank.species[f.type].right : Tank.mirror(Tank.species[f.type].right)
        const color = Tank.css(f.render)
        const base = k.t * 5.6 + f.phase
        let wave = Math.sin(base)
        let waveCos = Math.cos(base)
        const stepSin = Math.sin(0.85)
        const stepCos = Math.cos(0.85)
        // Each cell's wave is the previous one's phase plus FISH_SWIM_WAVE_SPACING.
        for (let c = 0; c < text.length; ++c) {
            if (text.charAt(c) !== " ")
                glyphAt(ctx, f.x + c * 6, f.y + Math.round(wave * 1.5), text.charAt(c), u, ox, oy, color)
            const next = wave * stepCos + waveCos * stepSin
            waveCos = waveCos * stepCos - wave * stepSin
            wave = next
        }
    }

    // Draw the gradient at final-pixel resolution. Fish remain on the original
    // 320x240 grid, while the 4x4 Bayer cells stay physically small on a large
    // desktop instead of scaling into visible color blocks.
    function drawWater(ctx, ox, oy, u) {
        const pixelWidth = Math.max(1, Math.round(worldW * u))
        const pixelHeight = Math.max(1, Math.round(worldH * u))
        const gradientHeight = Math.max(2, Math.round(80 * u))
        const cell = 4
        ctx.fillStyle = "#000000"
        ctx.fillRect(ox, oy, pixelWidth, pixelHeight)
        for (let y = 0; y < Math.min(pixelHeight, gradientHeight); y += cell) {
            const h = Math.min(cell, gradientHeight - y)
            let x = 0
            while (x < pixelWidth) {
                const color = Tank.waterAtPhysical(x, y, gradientHeight)
                let end = x + cell
                while (end < pixelWidth && Tank.waterAtPhysical(end, y, gradientHeight) === color)
                    end += cell
                ctx.fillStyle = Tank.css565(color)
                ctx.fillRect(ox + x, oy + y, end - x, h)
                x = end
            }
        }
    }

    function drawBackground() {
        if (!backgroundCanvas.available)
            return
        const ctx = backgroundCanvas.getContext("2d")
        ctx.reset()
        ctx.globalCompositeOperation = "source-over"
        ctx.globalAlpha = 1
        drawWater(ctx, 0, 0, p)
    }

    function requestBackgroundDraw() {
        if (backgroundCanvas.available && backgroundCanvas.width > 0 && backgroundCanvas.height > 0)
            backgroundCanvas.requestPaint()
    }

    function drawSeaweed(ctx, k, i, ox, oy, u) {
        const blade = k.seaweed[i]
        ctx.lineCap = "butt"
        let prev = { x: blade.x, y: worldH - 2 }
        for (let seg = 1; seg <= 7; ++seg) {
            const us = seg / 7
            const pt = seaweedPoint(k, us, i)
            const color = us < 0.38 ? Tank.named.DARKGREEN : us < 0.76 ? Tank.named.GREEN : Tank.named.GREENYELLOW
            ctx.strokeStyle = Tank.css(color)
            ctx.lineWidth = u
            ctx.beginPath()
            ctx.moveTo(ox + Math.round(prev.x * u), oy + Math.round(prev.y * u))
            ctx.lineTo(ox + Math.round(pt.x * u), oy + Math.round(pt.y * u))
            ctx.stroke()
            if (us < 0.78) {
                ctx.strokeStyle = Tank.css(Tank.named.DARKGREEN)
                ctx.beginPath()
                ctx.moveTo(ox + Math.round(prev.x * u) + u, oy + Math.round(prev.y * u))
                ctx.lineTo(ox + Math.round(pt.x * u) + u, oy + Math.round(pt.y * u))
                ctx.stroke()
            }
            prev = pt
        }
        const branches = clamp(Math.floor(blade.height / 14), 2, 5)
        for (let b = 0; b < branches; ++b) {
            let us = 0.30 + b * 0.14 + ((i + b) % 3) * 0.018
            if (us > 0.88)
                us = 0.88
            const pt = seaweedPoint(k, us, i)
            const side = ((i + b) & 1) ? 1 : -1
            const len = 5.5 + ((i * 3 + b * 5) % 5)
            const wiggle = Math.sin(k.t * (1.1 + i * 0.03) * 1.10 + i + b * 1.7) * 1.2
            ctx.strokeStyle = Tank.css((b & 1) ? Tank.named.DARKGREEN : Tank.named.GREEN)
            ctx.beginPath()
            ctx.moveTo(ox + Math.round(pt.x * u), oy + Math.round(pt.y * u))
            ctx.lineTo(ox + Math.round((pt.x + side * (len * 0.58 + Math.abs(pt.sway) * 0.05) + wiggle) * u),
                       oy + Math.round((pt.y - len * 0.78) * u))
            ctx.stroke()
        }
    }

    // drawOctopus: five glyphs and six waving tentacles; the deep one is a head
    // of eight and a row of tentacles.
    function drawOctopus(ctx, k, ox, oy, u) {
        const o = k.octopus
        if (!o.active)
            return
        const cx = Math.round(o.x)
        const cy = Math.round(o.y)
        if (o.cthulhu) {
            const r = Math.round(8 + 5 * Math.sin(k.t * 0.10 + o.colorPhase))
            const g = Math.round(82 + 24 * Math.sin(k.t * 0.13 + o.colorPhase + 1.7))
            const b = Math.round(34 + 10 * Math.sin(k.t * 0.09 + o.colorPhase + 3.1))
            const color = "rgb(" + r + "," + g + "," + b + ")"
            const headBob = Math.round(Math.sin(k.t * 1.1 + o.phase) * 1.4)
            const tentacleBob = Math.round(Math.sin(k.t * 1.8 + o.phase) * 1.6)
            glyphCenter(ctx, ox, oy, u, "(;,,;)", cx, cy + headBob, color)
            glyphCenter(ctx, ox, oy, u, (k.t * 3 % 2 | 0) ? "((()))" : "(()())", cx, cy + 15 + tentacleBob, color)
            return
        }
        const r = Math.round(205 + 42 * Math.sin(k.t * 0.18 + o.colorPhase))
        const g = Math.round(78 + 38 * Math.sin(k.t * 0.13 + o.colorPhase + 2.1))
        const b = Math.round(178 + 58 * Math.sin(k.t * 0.16 + o.colorPhase + 4.2))
        const color = "rgb(" + r + "," + g + "," + b + ")"
        const top = Math.round(Math.sin(k.t * 1.25 + o.phase) * 1.4)
        glyphAt(ctx, cx - 13, cy + top, "(", u, ox, oy, color)
        glyphAt(ctx, cx - 3, cy - 5, ".", u, ox, oy, color)
        glyphAt(ctx, cx + 7, cy - 5, ".", u, ox, oy, color)
        glyphAt(ctx, cx + 16, cy - top, ")", u, ox, oy, color)
        const xs = [-24, -16, -8, 2, 10, 18]
        const glyphs = ["(", "(", "(", ")", ")", ")"]
        for (let i = 0; i < 6; ++i) {
            const wave = Math.sin(k.t * 1.75 + o.phase + i * 0.72)
            glyphAt(ctx, cx + xs[i] + Math.round(wave * 1.4), cy + 13 + Math.round(wave * 2.2),
                    glyphs[i], u, ox, oy, color)
        }
    }

    // drawSeahorse: eight rows on 5x6 cells, mirrored glyph by glyph when it
    // swims right, three middle rows swaying, a fluttering fin.
    function drawSeahorse(ctx, k, ox, oy, u) {
        const s = k.seahorse
        if (!s.active)
            return
        const rows = ["  ^^  ", " / o) ", "[__-/ ", "  /|  ", " / |  ", " \\ |  ", "  ( ) ", "  \\_/ "]
        const x = Math.round(s.x)
        const y = Math.round(s.y)
        const sway = Math.round(Math.sin(k.t * 1.15 + s.phase) * 1.2)
        const flutter = Math.round(Math.sin(k.t * 10.0 + s.finPhase) * 1.2)
        const r = Math.round(238 + 12 * Math.sin(k.t * 0.11 + s.phase))
        const g = Math.round(142 + 18 * Math.sin(k.t * 0.16 + s.phase + 1.4))
        const b = Math.round(48 + 12 * Math.sin(k.t * 0.13 + s.phase + 2.8))
        const color = "rgb(" + r + "," + g + "," + b + ")"
        for (let row = 0; row < rows.length; ++row) {
            const rowSway = (row >= 1 && row <= 3) ? sway : 0
            for (let col = 0; col < 6; ++col) {
                let glyph = col < rows[row].length ? rows[row].charAt(col) : " "
                if (glyph === " ")
                    continue
                let drawCol = col
                if (s.facingRight) {
                    drawCol = 5 - col
                    glyph = Tank.mirrorChar(glyph)
                }
                glyphAt(ctx, x + drawCol * 5 + rowSway, y + row * 6, glyph, u, ox, oy, color)
            }
        }
        const finX = (s.facingRight ? x + 5 : x + 20) + flutter
        glyphCenter(ctx, ox, oy, u, Math.sin(k.t * 12.0 + s.finPhase) > 0 ? "~" : "-", finX, y + 24,
                    "rgb(255,188,82)")
    }

    // drawSnail: three rows of six on the firmware's floor line, the stalk on
    // its right side flicking between "|/" and "/|".
    function drawSnail(ctx, k, ox, oy, u) {
        const s = k.snail
        if (!s.active)
            return
        const r = Math.round(228 + 14 * Math.sin(k.t * 0.16 + s.phase))
        const g = Math.round(116 + 16 * Math.sin(k.t * 0.13 + s.phase + 1.3))
        const b = Math.round(34 + 8 * Math.sin(k.t * 0.11 + s.phase + 2.6))
        const color = "rgb(" + r + "," + g + "," + b + ")"
        const rows = [" __ " + (Math.sin(k.t * 1.6 + s.phase) > 0 ? "|/" : "/|"), "(@l)o", "`~~~~'"]
        const x = Math.round(s.x)
        const y = Math.round(s.y)
        for (let row = 0; row < rows.length; ++row) {
            const width = row === 0 ? 6 : 5
            for (let col = 0; col < width; ++col) {
                let glyph = rows[row].charAt(col)
                if (glyph === " ")
                    continue
                let drawCol = col
                if (s.facingRight) {
                    drawCol = width - 1 - col
                    glyph = Tank.mirrorChar(glyph)
                }
                glyphAt(ctx, x + drawCol * 6, y + row * 8, glyph, u, ox, oy, color)
            }
        }
    }

    // drawJellyfish: a green dome on a row of short legs, the legs brushing
    // with the pulse.
    function drawJellyfish(ctx, k, ox, oy, u) {
        const j = k.jellyfish
        if (!j.active)
            return
        const color = Tank.css565(Tank.named.SKYBLUE)
        const pulse = Math.sin(k.t * 1.55 + j.phase)
        const rows = [" .---. ", "( o o )", " `---' "] // the bell
        const x = Math.round(j.x)
        const y = Math.round(j.y)
        for (let row = 0; row < rows.length; ++row)
            for (let col = 0; col < 7; ++col)
                if (rows[row].charAt(col) !== " ")
                    glyphAt(ctx, x + col * 6, y + row * 8, rows[row].charAt(col), u, ox, oy, color)
        const legs = pulse > 0 ? "~|~|~" : "-|-|-"
        for (let col = 0; col < 5; ++col)
            glyphAt(ctx, x + 6 + col * 6, y + 24, legs.charAt(col), u, ox, oy, color)
    }

    // ---- the scene's plumbing ----------------------------------------------

    Canvas {
        id: backgroundCanvas
        z: 1
        x: root.originX
        y: root.originY
        width: root.boardW
        height: root.boardH
        renderStrategy: Canvas.Immediate
        antialiasing: false
        onAvailableChanged: if (available) root.requestBackgroundDraw()
        onPaint: root.drawBackground()
    }

    Canvas {
        id: canvas
        z: 2
        x: root.originX
        y: root.originY
        width: root.boardW
        height: root.boardH
        renderStrategy: Canvas.Immediate
        antialiasing: false
        // A hidden window's canvas is not available before its scene graph
        // exists, so this is also where the tank is first painted.
        onAvailableChanged: if (available) { root.lastTick = Date.now(); root.requestDraw() }
        onPaint: root.draw()
    }

    // A tap on the water drops one flake where it landed, on the tank under
    // the pointer. The MouseArea takes no focus, so the cover's keys still
    // reach the exit sequence.
    MouseArea {
        anchors.fill: parent
        acceptedButtons: Qt.LeftButton
        onPressed: (mouse) => {
            spawnFlake(root.tanks[0], (mouse.x - originX) / p, (mouse.y - originY) / p)
            root.paint()
        }
    }

    Timer {
        running: root.cover.shown
        interval: root.tickMs
        repeat: true
        onTriggered: root.step()
    }

    onWidthChanged: { requestBackgroundDraw(); paint() }
    onHeightChanged: { requestBackgroundDraw(); paint() }

    Component.onCompleted: {
        root.lastTick = Date.now()
        root.resizeTanks()
        root.cover.shownChanged.connect(function() {
            if (root.cover.shown)
                root.reset()
        })
    }
}
