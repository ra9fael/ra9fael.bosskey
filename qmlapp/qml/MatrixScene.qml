// MatrixScene.qml - falling code columns, updated only while the cover is shown.
// The columns mix halfwidth katakana with digits, Latin letters and Chinese, so
// the rain reads as code whichever way the boss looks at it. Wide glyphs are
// centred in the cell: a column pitch stays uniform even where the scripts
// change width.
import QtQuick

Item {
    id: root

    required property var cover
    required property int screenIndex

    readonly property int cellHeight: Math.max(16, Math.round(height / 48))
    // Wide cells: a Chinese glyph advances a full em, and it has to fit inside
    // the column pitch without crowding its neighbours.
    readonly property int cellWidth: Math.max(16, Math.round(cellHeight * 1.15))
    readonly property int fontPx: Math.round(cellHeight * 0.72)
    readonly property string narrowGlyphs: "ｱｲｳｴｵｶｷｸｹｺｻｼｽｾｿﾀﾁﾂﾃﾄﾅﾆﾇﾈﾉﾊﾋﾌﾍﾎﾏﾐﾑﾒﾓﾔﾕﾖﾗﾘﾙﾚﾛﾜﾝ0123456789ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz"
    readonly property string wideGlyphs: "系统核心线程数据包网络协议错误警告编译运行加载缓存索引队列同步异步加密解密端口进程内存磁盘文件句柄指令跳转循环函数对象模块插件服务请求响应会话令牌校验权限日志配置参数变量常量指针引用结构枚举模板异常捕获释放构建检测监控遍历递归迭代"

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

    // A glyph index into narrowGlyphs + wideGlyphs; the split keeps the rain
    // mostly halfwidth with Chinese turning up often enough to be noticed.
    function pickGlyph(rand) {
        return rand() < 0.62
            ? Math.floor(rand() * narrowGlyphs.length)
            : narrowGlyphs.length + Math.floor(rand() * wideGlyphs.length)
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
                                   () => pickGlyph(rand))
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
            ctx.font = "bold " + root.fontPx + "px 'MS Gothic', 'Microsoft YaHei', 'SimSun', monospace"
            ctx.textBaseline = "top"

            const narrow = root.narrowGlyphs
            const wide = root.wideGlyphs
            // Two measurements a frame: every glyph of a script advances alike.
            const narrowAdvance = ctx.measureText(narrow.charAt(0)).width
            const wideAdvance = ctx.measureText(wide.charAt(0)).width
            const cell = root.cellWidth

            for (let c = 0; c < root.columns.length; ++c) {
                const column = root.columns[c]
                const head = Math.floor(column.head)
                const x = c * cell
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
                    const code = column.glyphs[index]
                    const isWide = code >= narrow.length
                    const advance = isWide ? wideAdvance : narrowAdvance
                    ctx.fillText(isWide ? wide.charAt(code - narrow.length)
                                        : narrow.charAt(code),
                                 x + (cell - advance) * 0.5, row * root.cellHeight)
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
                        column.glyphs[g] = root.pickGlyph(root.random)
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
