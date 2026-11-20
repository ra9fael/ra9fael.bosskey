// test_logic.cpp - the pure logic of Boss Key (logic.h): the scene map, the
// exit-sequence matcher, the percent pacing, the gcc log; and the QR encoder
// (qrcode.h). No Qt, no windows.
#include "logic.h"
#include "qrcode.h"

#include <cstdio>
#include <cstdlib>

namespace {

int g_failures = 0;

void expect(bool ok, const char* what)
{
    if (!ok) {
        std::printf("FAIL: %s\n", what);
        ++g_failures;
    }
}

void expectEq(const std::string& got, const std::string& want, const char* what)
{
    if (got != want) {
        std::printf("FAIL: %s: got '%s', want '%s'\n", what, got.c_str(), want.c_str());
        ++g_failures;
    }
}

// ---- the scene map -----------------------------------------------------------

void testSceneMap()
{
    const auto map = bosskey::parseSceneMap("b=bsod, u=update;t=gcc");
    expect(map.size() == 3, "map: three valid pairs");
    expectEq(map[0].token, "b", "map: first token");
    expectEq(map[0].scene, "bsod", "map: first scene");
    expectEq(map[2].scene, "gcc", "map: third scene");

    // Case and spaces are normalized; duplicates collapse to the first.
    const auto folded = bosskey::parseSceneMap("B = BSOD , b=gcc");
    expect(folded.size() == 1, "map: case-folded and deduplicated");
    expectEq(folded[0].scene, "bsod", "map: first binding wins");

    // Junk is skipped, not fatal; a wrong scene id does not enter the map.
    expect(bosskey::parseSceneMap("x=purple,,=bsod,verylongtoken=bsod").empty(),
           "map: invalid pairs skipped");

    // Empty or unparsable text gives the empty map (callers fall back).
    expect(bosskey::parseSceneMap("").empty(), "map: empty text");
    expect(bosskey::parseSceneMap("nothing here").empty(), "map: no pairs");

    // The default map binds the three documented tokens.
    const auto def = bosskey::defaultSceneMap();
    expect(def.size() == 5, "default map: five tokens");
    expectEq(bosskey::resolveScene(def, "b"), bosskey::kSceneBsod, "resolve: b -> bsod");
    expectEq(bosskey::resolveScene(def, "T"), bosskey::kSceneGcc, "resolve: case-folded");
    expectEq(bosskey::resolveScene(def, "m"), bosskey::kSceneMatrix, "resolve: m -> matrix");
    expect(bosskey::isKnownScene(bosskey::kSceneMatrix), "scene: matrix is known");
    expect(bosskey::isKnownScene(bosskey::kSceneBlackhole), "scene: blackhole is known");
    expectEq(bosskey::resolveScene(def, "h"), bosskey::kSceneBlackhole,
             "resolve: h -> blackhole");
    const auto allScenes = bosskey::parseSceneMap(
        "b=bsod,u=update,t=gcc,m=matrix,h=blackhole");
    expect(allScenes.size() == 5, "map: all five documented scenes parse");
    expectEq(bosskey::resolveScene(def, "x"), "", "resolve: unbound token");
    expectEq(bosskey::resolveScene(def, ""), "", "resolve: empty word");
    expectEq(bosskey::resolveScene(def, "b u"), "", "resolve: two words refused");
    expectEq(bosskey::resolveScene(bosskey::parseSceneMap(""), "b"), "",
             "resolve: empty map binds nothing");
}

// ---- the exit sequence -------------------------------------------------------

void testSequence()
{
    expectEq(bosskey::normalizeSequence("Dead-Beef!"), "deadbeef", "sequence: normalized");
    expectEq(bosskey::normalizeSequence(""), "", "sequence: empty stays empty");

    std::string buffer;
    bool hit = false;
    for (const char c : std::string("xxdeadbeef")) {
        auto [next, now] = bosskey::pushKey(std::move(buffer), "deadbeef", c);
        buffer = std::move(next);
        hit = hit || now;
    }
    expect(hit, "sequence: deadbeef detected at the end");

    buffer.clear();
    hit = false;
    for (const char c : std::string("deadbee")) {
        auto [next, now] = bosskey::pushKey(std::move(buffer), "deadbeef", c);
        buffer = std::move(next);
        hit = hit || now;
    }
    expect(!hit, "sequence: prefix alone does not fire");
    auto [withF, fired] = bosskey::pushKey(std::move(buffer), "deadbeef", 'F');
    expect(fired, "sequence: fires across calls, case-insensitive");
    expectEq(withF, "deadbeef", "sequence: buffer keeps its history");

    // A long buffer never grows without bound.
    std::string longBuffer;
    for (int i = 0; i < 100; ++i) {
        auto [next, now] = bosskey::pushKey(std::move(longBuffer), "deadbeef", 'a');
        longBuffer = std::move(next);
        expect(!now, "sequence: 'a's never fire");
    }
    expect(longBuffer.size() <= bosskey::kSequenceBufferKeep, "sequence: buffer capped");

    // Keys outside the alphabet are ignored entirely.
    auto [kept, none] = bosskey::pushKey("abc", "deadbeef", ' ');
    expect(!none, "sequence: space ignored");
    expectEq(kept, "abc", "sequence: space leaves the buffer");

    // An empty sequence never fires (safety valve), but the buffer still fills.
    auto [b2, never] = bosskey::pushKey("", "", 'd');
    expect(!never, "sequence: empty sequence disabled");
    expectEq(b2, "d", "sequence: empty sequence keeps the key only");
}

// ---- the percent pacing ------------------------------------------------------

void testPercent()
{
    expect(bosskey::coverPercent(-5, bosskey::kSceneBsod) == 0, "percent: negative time clamps");
    expect(bosskey::coverPercent(0, bosskey::kSceneUpdate) == 0, "percent: zero time");

    // Monotonic, in range, never jumping backwards, for both scenes.
    for (const char* scene : {bosskey::kSceneBsod, bosskey::kSceneUpdate}) {
        int previous = -1;
        for (int ms = 0; ms <= 400000; ms += 500) {
            const int value = bosskey::coverPercent(ms, scene);
            expect(value >= previous && value <= 100, "percent: monotonic and capped");
            previous = value;
        }
    }
    // The update scene stalls at the classic 87 (from 55 s, then a slow crawl).
    expect(bosskey::coverPercent(56000, bosskey::kSceneUpdate) == 87,
           "percent: update stalls at 87");
    // The blue screen collects slowly, still below 100 minutes in only just.
    expect(bosskey::coverPercent(60000, bosskey::kSceneBsod) < 100, "percent: bsod not done at 1 min");
    expect(bosskey::coverPercent(600000, bosskey::kSceneBsod) == 100, "percent: bsod done at 10 min");
}

// ---- the gcc log -------------------------------------------------------------

void testGccLog()
{
    const auto log = bosskey::gccLog("winshun-core", 12345);
    expect(log.size() > 20, "gcc: a build pass's worth of lines");
    expect(log.front().find("ninja") != std::string::npos, "gcc: starts at the invocation");
    expect(log.size() >= 3 && log[log.size() - 3].find("Re-running CMake") != std::string::npos
               && log.back() == "-- Generating done",
           "gcc: hands over to the next pass instead of linking");

    bool linked = false;
    std::size_t warnings = 0;
    int topPercent = 0;
    bool hundred = false;
    for (const std::string& line : log) {
        linked = linked || line.find("Built target") != std::string::npos
                        || line.find("Linking") != std::string::npos;
        warnings += line.find("warning:") != std::string::npos;
        if (line.find('[') == 0) {
            const int percent = std::atoi(line.c_str() + 1);
            topPercent = std::max(topPercent, percent);
            hundred = hundred || percent >= 100;
        }
    }
    expect(!linked, "gcc: never links: the build has no end");
    expect(!hundred, "gcc: a pass never reaches 100%");
    expect(topPercent > 90, "gcc: a pass climbs deep before it hands over");
    expect(warnings >= 1, "gcc: at least one warning");

    // Deterministic in the seed; different seeds differ.
    const auto again = bosskey::gccLog("winshun-core", 12345);
    expect(again.size() == log.size(), "gcc: deterministic size");
    bool same = again.size() == log.size();
    for (std::size_t i = 0; same && i < log.size(); ++i)
        same = again[i] == log[i];
    expect(same, "gcc: deterministic content");
    expect(bosskey::gccLog("winshun-core", 999).size() != log.size()
               || bosskey::gccLog("other", 12345) != log,
           "gcc: seed or project changes the log");

    // Only the first pass carries the invocation; later ones continue the build.
    const auto next = bosskey::gccLog("winshun-core", 12345, 1);
    expect(next.front().find("ninja") == std::string::npos, "gcc: a later pass has no header");
    expect(next.size() >= 3 && next[next.size() - 3].find("Re-running CMake") != std::string::npos
               && next.back() == "-- Generating done",
           "gcc: a later pass also hands over");

    // The log streams for as long as the cover is up: pass after pass, and not
    // one line claims the build is done.
    bool everFinished = false;
    for (int pass = 0; pass < 40 && !everFinished; ++pass) {
        for (const std::string& line : bosskey::gccLog("winshun-core", 12345, pass)) {
            if (line.find("Built target") != std::string::npos
                || line.find("Linking") != std::string::npos
                || line.find("100%") != std::string::npos) {
                everFinished = true;
                break;
            }
        }
    }
    expect(!everFinished, "gcc: no pass ever finishes the build");

    // An empty project name falls back to a safe target.
    expect(bosskey::gccLog("", 7).size() > 20, "gcc: empty project still builds");
}

// ---- the QR code -------------------------------------------------------------

namespace {

char cellAt(const std::string& rows, int row, int col)
{
    return rows[static_cast<std::size_t>(row) * (bosskey::qr::kSize + 1) + col];
}

} // namespace

void testQr()
{
    using namespace bosskey::qr;

    const std::string url = "https://www.windows.com/stopcode";
    const Code code = encode(url);
    expect(code.ok, "qr: the stopcode URL fits");
    expect(code.size == 29, "qr: version 3 is 29 modules");
    expect(code.rows.size() == 29 * 30 - 1, "qr: 29 rows of 29 joined by newlines");

    // The three finder patterns: a 7x7 ring around a 3x3 core, white
    // separators outside. These are what makes it read as a QR code at a
    // glance, so they are the first thing to pin down.
    for (const auto& [top, left] : {std::pair{0, 0}, std::pair{0, 22}, std::pair{22, 0}}) {
        expect(cellAt(code.rows, top + 0, left + 0) == '1', "qr: finder outer ring");
        expect(cellAt(code.rows, top + 1, left + 1) == '0', "qr: finder white ring");
        expect(cellAt(code.rows, top + 2, left + 2) == '1', "qr: finder core");
        expect(cellAt(code.rows, top + 3, left + 3) == '1', "qr: finder core filled");
        expect(cellAt(code.rows, top + 6, left + 6) == '1', "qr: finder outer corner");
    }
    // The separators sit on the inner sides of the three corners.
    expect(cellAt(code.rows, 7, 7) == '0', "qr: finder separator, top left");
    expect(cellAt(code.rows, 7, 21) == '0', "qr: finder separator, top right");
    expect(cellAt(code.rows, 21, 7) == '0', "qr: finder separator, bottom left");

    // The timing patterns alternate on row 6 and column 6.
    expect(cellAt(code.rows, 6, 8) == '1', "qr: timing starts dark");
    expect(cellAt(code.rows, 6, 9) == '0', "qr: timing alternates");
    expect(cellAt(code.rows, 8, 6) == '1', "qr: vertical timing");
    expect(cellAt(code.rows, 9, 6) == '0', "qr: vertical timing alternates");

    // The alignment pattern (Version 3's single one) and the dark module.
    expect(cellAt(code.rows, 22, 22) == '1', "qr: alignment centre");
    expect(cellAt(code.rows, 20, 20) == '1', "qr: alignment outer ring");
    expect(cellAt(code.rows, 21, 21) == '0', "qr: alignment white ring");
    expect(cellAt(code.rows, 21, 8) == '1', "qr: dark module");

    // Deterministic, and different input gives a different matrix.
    const Code again = encode(url);
    expect(again.rows == code.rows, "qr: deterministic");
    expect(encode("https://www.windows.com/").rows != code.rows, "qr: input changes the code");

    // The empty string is legal; past the 53-byte payload it is refused.
    expect(encode("").ok, "qr: empty text encodes");
    expect(!encode(std::string(kDataCodewords + 1, 'a')).ok, "qr: overlong text refused");

    // The verify harness (tools/verify/qr_check.py) decodes the matrix from
    // this dump with an independent reader.
    if (const char* path = std::getenv("BOSSKEY_QR_DUMP")) {
        if (FILE* f = std::fopen(path, "wb")) {
            std::fwrite(code.rows.data(), 1, code.rows.size(), f);
            std::fclose(f);
            std::printf("qr: matrix written to %s\n", path);
        }
    }
}

} // namespace

int main()
{
    testSceneMap();
    testSequence();
    testPercent();
    testGccLog();
    testQr();
    if (g_failures == 0) {
        std::printf("all bosskey logic tests passed\n");
        return 0;
    }
    std::printf("%d failure(s)\n", g_failures);
    return 1;
}
