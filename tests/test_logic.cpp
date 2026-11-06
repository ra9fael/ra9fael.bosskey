// test_logic.cpp - the pure logic of Boss Key (logic.h): the scene map, the
// exit-sequence matcher, the percent pacing, the gcc log. No Qt, no windows.
#include "logic.h"

#include <cstdio>

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
    expect(def.size() == 3, "default map: three tokens");
    expectEq(bosskey::resolveScene(def, "b"), bosskey::kSceneBsod, "resolve: b -> bsod");
    expectEq(bosskey::resolveScene(def, "T"), bosskey::kSceneGcc, "resolve: case-folded");
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
    expect(log.size() > 20, "gcc: a full build's worth of lines");
    expect(log.front().find("ninja") != std::string::npos, "gcc: starts at the invocation");
    expect(log.back().find("$") != std::string::npos, "gcc: ends at a prompt");

    bool linked = false;
    std::size_t warnings = 0;
    for (const std::string& line : log) {
        linked = linked || line.find("Built target winshun-core") != std::string::npos;
        warnings += line.find("warning:") != std::string::npos;
    }
    expect(linked, "gcc: links the configured target");
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

    // An empty project name falls back to a safe target.
    expect(bosskey::gccLog("", 7).size() > 20, "gcc: empty project still builds");
}

} // namespace

int main()
{
    testSceneMap();
    testSequence();
    testPercent();
    testGccLog();
    if (g_failures == 0) {
        std::printf("all bosskey logic tests passed\n");
        return 0;
    }
    std::printf("%d failure(s)\n", g_failures);
    return 1;
}
