// Aquarium.js - POWER-PILL/ASCII-Aquarium v2.39's tables and colour maths
// (MIT), apart from the scene.
//
// The firmware drew through TFT_eSPI, and three of its behaviours decide what
// the tank looks like: colours are 5-6-5 and are scaled into ABGR565 at 8-bit
// precision, a fish mirrors glyph by glyph so its tail leads when it swims
// left, and every glyph sits in a 6 by 8 cell. All three are reproduced here
// at the firmware's own arithmetic, so a colour or a fish is the screen's.
.pragma library

// rgb565From888: five bits red, six green, five blue.
function rgb565(r, g, b) {
    return ((r & 0xF8) << 8) | ((g & 0xFC) << 3) | ((b >> 3) & 0x1F)
}

// TFT_eSPI's 256-colour palette, from its red (5-bit), green (6-bit) and
// blue (5-bit) tables: values 0..255, every one eight wide.
var palette = (function() {
    var red = [0, 8, 16, 25, 33, 41, 49, 58, 66, 74, 82, 90, 99, 107, 115, 123,
               132, 140, 148, 156, 165, 173, 181, 189, 197, 206, 214, 222, 230, 239, 247, 255]
    var green = [0, 4, 8, 12, 16, 20, 24, 28, 32, 36, 40, 45, 49, 53, 57, 61,
                 65, 69, 73, 77, 81, 85, 89, 93, 97, 101, 105, 109, 113, 117, 121, 125,
                 130, 134, 138, 142, 146, 150, 154, 158, 162, 166, 170, 174, 178, 182, 186, 190,
                 194, 198, 202, 206, 210, 215, 219, 223, 227, 231, 235, 239, 243, 247, 251, 255]
    var table = []
    for (var c = 0; c < 8; ++c) {
        for (var p = 0; p < 32; ++p)
            table.push(rgb565(red[c * 4], green[c * 4 + 2], red[p]))
        for (var q = 0; q < 32; ++q)
            table.push(rgb565(red[c * 4 + 2], green[c * 4 + 3], red[q]))
    }
    return table
})()

// The firmware's named colours, from TFT_WHITE through TFT_ORANGE.
var named = {
    DARKGREEN: rgb565(0, 128, 0), DARKGREY: rgb565(128, 128, 128), BLUE: rgb565(0, 0, 255),
    RED: rgb565(255, 0, 0), GREEN: rgb565(0, 255, 0), CYAN: rgb565(0, 255, 255),
    MAGENTA: rgb565(255, 0, 255), YELLOW: rgb565(255, 255, 0), WHITE: rgb565(255, 255, 255),
    ORANGE: rgb565(255, 165, 0), GREENYELLOW: rgb565(173, 255, 47), PINK: rgb565(255, 192, 203),
    VIOLET: rgb565(238, 130, 238), SKYBLUE: rgb565(135, 206, 235), GOLD: rgb565(255, 215, 0)
}

// FishSpecies: the twelve glyphs, right-facing, in the firmware's base colours.
var species = [
    { right: "><>", color: rgb565(80, 200, 120) },
    { right: ">)))'>", color: rgb565(0, 150, 255) },
    { right: "oO0", color: named.PINK },
    { right: "><((( '>", color: rgb565(255, 184, 0) },
    { right: "~~{o}", color: named.VIOLET },
    { right: "><(((o>", color: named.RED },
    { right: "><((((>`", color: named.ORANGE },
    { right: "><((( '>", color: rgb565(0, 180, 170) },
    { right: "}>{{{{* >", color: rgb565(75, 0, 156) },
    { right: "><((( *>", color: rgb565(200, 120, 255) },
    { right: ">(')>", color: rgb565(255, 158, 200) },
    { right: ">'>", color: named.YELLOW }
]

// kAltFishColors: the hues one fish in five gets instead of its own.
var altColors = [named.CYAN, named.MAGENTA, named.WHITE, named.SKYBLUE, named.GOLD,
                 named.ORANGE, named.GREENYELLOW, named.DARKGREY]

// The firmware measured each species with textWidth and drew every character
// its own advance apart; here the advance is one 6-pixel cell, the GLCD font's
// own, so a species is as wide as its characters are long.
var cellW = 6

function advance(right) {
    return right.length * cellW
}

function mirrorAsciiBracket(c) {
    switch (c) {
    case ">": return "<"
    case "<": return ">"
    case "(": return ")"
    case ")": return "("
    case "{": return "}"
    case "}": return "{"
    case "[": return "]"
    case "]": return "["
    }
    return c
}

// The seahorse's and the snail's mirror: the brackets again, and the slashes.
function mirrorChar(c) {
    switch (c) {
    case "/": return "\\"
    case "\\": return "/"
    case "<": return ">"
    case ">": return "<"
    }
    return mirrorAsciiBracket(c)
}

function mirror(right) {
    var out = ""
    for (var i = right.length - 1; i >= 0; --i)
        out += mirrorAsciiBracket(right.charAt(i))
    return out
}

// scaleRgb565: each channel scaled at 8 bits and re-packed as ABGR565.
function scaleRgb565(color, brightness) {
    var b = Math.round((color & 0x1F) * brightness)
    var g = Math.round(((color >> 5) & 0x3F) * brightness)
    var r = Math.round((color >> 11) * brightness)
    return (Math.min(b, 31) << 16) | (Math.min(g, 63) << 5) | Math.min(r, 31)
}

// The CSS colour of an ABGR565 value, in the sixteen-bit RGB565 it came from.
function css(abgr) {
    return "rgb(" + palette[(abgr & 0x1F) << 3] + "," + palette[((abgr >> 5) & 0x3F) << 2]
        + "," + palette[((abgr >> 16) & 0x1F) << 3] + ")"
}

function css565(value) {
    return "rgb(" + palette[((value >> 11) & 0x1F) << 3] + "," + palette[((value >> 5) & 0x3F) << 2]
        + "," + palette[(value & 0x1F) << 3] + ")"
}

// randomFoodColor: orange, its brightness and tint varying a little.
function randomFoodColor(random) {
    return rgb565(irand(random, 220, 255), irand(random, 118, 165), irand(random, 0, 33))
}

// randomBubbleColor: a pale blue, barely there.
function randomBubbleColor(random) {
    return rgb565(irand(random, 0, 4), irand(random, 8, 23), irand(random, 45, 105))
}

// The firmware's random(low, high): high is exclusive.
function irand(random, low, high) {
    return low + Math.floor(random() * (high - low))
}

// Depth shading: 28% far (0.48..0.64), 42% mid (0.66..0.84), 30% near (0.88..1).
function randomDepth(random) {
    var roll = irand(random, 0, 100)
    if (roll < 28)
        return 0.48 + random() * 0.16
    if (roll < 70)
        return 0.66 + random() * 0.18
    return 0.88 + random() * 0.12
}

// TFT_eSPI's ordered, dithered RGB565 gradient: the complete blue channel fades
// with green to black. Sampling, interpolation, and Bayer thresholds follow
// the firmware's integer arithmetic so its characteristic pixel texture stays.
var bayer8 = [
     0, 48, 12, 60,  3, 51, 15, 63,
    32, 16, 44, 28, 35, 19, 47, 31,
     8, 56,  4, 52, 11, 59,  7, 55,
    40, 24, 36, 20, 43, 27, 39, 23,
     2, 50, 14, 62,  1, 49, 13, 61,
    34, 18, 46, 30, 33, 17, 45, 29,
    10, 58,  6, 54,  9, 57,  5, 53,
    42, 26, 38, 22, 41, 25, 37, 21
]
var gradientStops = [0, 18, 42, 74, 112, 156, 204, 232, 255]
var gradientBrightness = [255, 228, 198, 164, 126, 90, 58, 30, 0]
var gradientRows = 80
var gradientTop = rgb565(0, 8, 255)
var gradientColors = (function() {
    var colors = []
    var topGreen = channel6To8((gradientTop >> 5) & 0x3F)
    var topBlue = color5To8(gradientTop & 0x1F)
    for (var i = 0; i < gradientBrightness.length; ++i) {
        var level = gradientBrightness[i]
        colors.push(rgb565(0, (topGreen * level / 255) | 0, (topBlue * level / 255) | 0))
    }
    return colors
})()

var asciiClockGlyphs = {
    "0": ["  ___", " / _ \\", "| | | |", "| |_| |", " \\___/"],
    "1": [" _", "/ |", "| |", "| |", "|_|"],
    "2": [" ____", "|___ \\", "  __) |", " / __/", "|_____|"],
    "3": [" _____", "|___ /", "  |_ \\", " ___) |", "|____/"],
    "4": [" _  _", "| || |", "| || |_", "|__   _|", "   |_|"],
    "5": [" ____", "| ___|", "|___ \\", " ___) |", "|____/"],
    "6": ["  __", " / /_", "| '_ \\", "| (_) |", " \\___/"],
    "7": [" _____", "|___  |", "   / /", "  / /", " /_/"],
    "8": ["  ___", " ( _ )", " / _ \\", "| (_) |", " \\___/"],
    "9": ["  ___", " / _ \\", "| (_) |", " \\__, |", "  /_/"],
    ":": [" ", " _", "(_)", " _", "(_)"],
    " ": [" ", " ", " ", " ", " "]
}

function asciiClockWidth(character) {
    var glyph = asciiClockGlyphs[character] || asciiClockGlyphs[" "]
    var width = 1
    for (var i = 0; i < glyph.length; ++i)
        width = Math.max(width, glyph[i].length)
    return width
}

function color5To8(value) {
    return (value << 3) | (value >> 2)
}

function channel6To8(value) {
    return (value << 2) | (value >> 4)
}

function rgbAtGradientT(t255) {
    t255 = Math.max(0, Math.min(255, t255))
    var index = 0
    while (index < gradientStops.length - 1 && t255 > gradientStops[index])
        ++index
    if (index === 0)
        return gradientColors[0]
    if (t255 >= gradientStops[gradientStops.length - 1])
        return gradientColors[gradientColors.length - 1]
    var t0 = gradientStops[index - 1]
    var t1 = gradientStops[index]
    var blend = ((t255 - t0) * 255 / (t1 - t0)) | 0
    var inverse = 255 - blend
    var c0 = gradientColors[index - 1]
    var c1 = gradientColors[index]
    var r0 = color5To8((c0 >> 11) & 0x1F)
    var g0 = channel6To8((c0 >> 5) & 0x3F)
    var b0 = color5To8(c0 & 0x1F)
    var r1 = color5To8((c1 >> 11) & 0x1F)
    var g1 = channel6To8((c1 >> 5) & 0x3F)
    var b1 = color5To8(c1 & 0x1F)
    var r = (r0 * inverse + r1 * blend) / 255 | 0
    var g = (g0 * inverse + g1 * blend) / 255 | 0
    var b = (b0 * inverse + b1 * blend) / 255 | 0
    return rgb565(r, g, b)
}

// One tank pixel of the v2.39 background. The firmware samples every row,
// then applies a scaled 8x8 Bayer threshold to the gradient coordinate.
function waterAt(x, y, height) {
    var rows = height || gradientRows
    if (y >= rows)
        return 0
    var base = ((y * 255) / (rows - 1)) | 0
    var threshold = bayer8[(Math.floor(y / 4) & 7) * 8 + (Math.floor(x / 4) & 7)] << 2
    var offset = ((threshold - 128) * 28 / 128) | 0
    var t = Math.max(0, Math.min(255, base + offset))
    return rgbAtGradientT(t)
}

// The desktop renderer keeps the firmware's gradient height in source-space,
// but samples the ordered pattern in final pixels. This prevents a 4x4 Bayer
// cell from becoming a large solid block when the 320x240 tank is enlarged.
function waterAtPhysical(x, y, height) {
    if (y >= height)
        return 0
    var base = ((y * 255) / (height - 1)) | 0
    var threshold = bayer8[(((y / 4) | 0) & 7) * 8 + (((x / 4) | 0) & 7)] << 2
    var offset = ((threshold - 128) * 28 / 128) | 0
    return rgbAtGradientT(Math.max(0, Math.min(255, base + offset)))
}
