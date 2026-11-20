// MatrixScene.qml - falling code columns, updated only while the cover is shown.
import QtQuick

Item {
    id: root

    required property var cover
    required property int screenIndex

    readonly property int cellHeight: Math.max(16, Math.round(height / 48))
    readonly property int cellWidth: Math.max(12, Math.round(cellHeight * 0.82))
    property var columns: []
    property int frame: 0
    property var random: Math.random

    function randomFor(seed) {
        let value = seed >>> 0
        return function() {
            value = (value * 1664525 + 1013904223) >>> 0
            return value / 4294967296
        }
    }

    function initColumns() {
        const rand = randomFor(0x4d415452 + (screenIndex + 1) * 7919 + frame)
        random = rand
        const count = Math.ceil(width / cellWidth) + 1
        const next = []
        for (let i = 0; i < count; ++i) {
            next.push({
                head: -rand() * height / cellHeight,
                speed: 3 + rand() * 8,
                length: 8 + Math.floor(rand() * 22),
                glyphs: Array.from({ length: Math.ceil(height / cellHeight) + 32 },
                                   () => Math.floor(rand() * 55))
            })
        }
        columns = next
        rain.requestPaint()
    }

    Rectangle {
        anchors.fill: parent
        color: "#010603"
    }

    Canvas {
        id: rain
        anchors.fill: parent
        renderStrategy: Canvas.Immediate

        onPaint: {
            const ctx = getContext("2d")
            ctx.reset()
            ctx.fillStyle = "#010603"
            ctx.fillRect(0, 0, width, height)
            ctx.font = "bold " + Math.round(root.cellHeight * 0.86) + "px MS Gothic"
            ctx.textBaseline = "top"

            const glyphs = "ｱｲｳｴｵｶｷｸｹｺｻｼｽｾｿﾀﾁﾂﾃﾄﾅﾆﾇﾈﾉﾊﾋﾌﾍﾎﾏﾐﾑﾒﾓﾔﾕﾖﾗﾘﾙﾚﾛﾜﾝ0123456789"
            for (let c = 0; c < root.columns.length; ++c) {
                const column = root.columns[c]
                const head = Math.floor(column.head)
                const x = c * root.cellWidth
                for (let trail = 0; trail < column.length; ++trail) {
                    const row = head - trail
                    if (row < 0 || row * root.cellHeight >= height)
                        continue
                    const age = trail / column.length
                    ctx.globalAlpha = 1 - age * 0.88
                    ctx.fillStyle = trail === 0 ? "#E8FFE9"
                                  : trail === 1 ? "#75FF8B" : "#00D64B"
                    const glyphIndex = Math.floor(column.head) - trail
                    const index = ((glyphIndex % column.glyphs.length) + column.glyphs.length)
                        % column.glyphs.length
                    ctx.fillText(glyphs.charAt(index), x, row * root.cellHeight)
                }
            }
            ctx.globalAlpha = 1
        }
    }

    Timer {
        running: root.cover.shown
        interval: 100
        repeat: true
        onTriggered: {
            ++root.frame
            const dt = interval / 1000
            const next = root.columns.slice()
            for (let i = 0; i < next.length; ++i) {
                const column = next[i]
                column.head += column.speed * dt
                if (column.head - column.length > root.height / root.cellHeight) {
                    column.head = -root.random() * 18
                    column.speed = 3 + root.random() * 8
                    column.length = 8 + Math.floor(root.random() * 22)
                    for (let g = 0; g < column.glyphs.length; ++g)
                        column.glyphs[g] = Math.floor(root.random() * 55)
                }
            }
            root.columns = next
            rain.requestPaint()
        }
    }

    onWidthChanged: initColumns()
    onHeightChanged: initColumns()
    Component.onCompleted: initColumns()
}
