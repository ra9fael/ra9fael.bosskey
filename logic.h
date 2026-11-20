// logic.h - Boss Key's pure logic: no Qt, no OS calls, so the tests run it
// anywhere. Everything the cover shows that must behave (scene map parsing,
// the exit-sequence matcher, the fake percent pacing, the gcc log) lives here;
// the QML side only displays.
//
// Code and comments are English; user-visible strings stay in the callers.
#ifndef BOSSKEY_LOGIC_H
#define BOSSKEY_LOGIC_H

#include <algorithm>
#include <cctype>
#include <cstdio>
#include <string>
#include <utility>
#include <vector>

namespace bosskey {

// The three scenes. Their ids are stable: plugin.json's sceneMap default and
// the QML loader both speak them.
constexpr const char* kSceneBsod = "bsod";
constexpr const char* kSceneUpdate = "update";
constexpr const char* kSceneGcc = "gcc";
constexpr const char* kSceneMatrix = "matrix";
constexpr const char* kSceneBlackhole = "blackhole";

inline bool isKnownScene(const std::string& scene)
{
    return scene == kSceneBsod || scene == kSceneUpdate || scene == kSceneGcc
        || scene == kSceneMatrix || scene == kSceneBlackhole;
}

inline std::string lower(std::string s)
{
    std::transform(s.begin(), s.end(), s.begin(),
                   [](unsigned char c) { return static_cast<char>(std::tolower(c)); });
    return s;
}

inline std::string trim(const std::string& s)
{
    const auto a = s.find_first_not_of(" \t\r\n");
    if (a == std::string::npos)
        return {};
    const auto b = s.find_last_not_of(" \t\r\n");
    return s.substr(a, b - a + 1);
}

// ---- settings ---------------------------------------------------------------

struct SceneBinding {
    std::string token; // what the user types after the keyword, e.g. "b"
    std::string scene; // kScene* id
};

// Parses "b=bsod, u=update, t=gcc" (spaces and ; | newlines as separators
// too). Invalid pairs are skipped; empty input gives the empty map, and the
// caller falls back to the defaults.
inline std::vector<SceneBinding> parseSceneMap(const std::string& text)
{
    std::vector<SceneBinding> out;
    std::string item;
    auto flush = [&] {
        item = trim(item);
        const auto eq = item.find('=');
        if (eq == std::string::npos || eq == 0 || eq + 1 == item.size())
            return;
        const std::string token = lower(trim(item.substr(0, eq)));
        const std::string scene = lower(trim(item.substr(eq + 1)));
        // One character per token keeps the keyword line readable ("boss b");
        // longer tokens would fight the query box.
        if (token.size() != 1 || !std::isalnum(static_cast<unsigned char>(token[0])))
            return;
        if (!isKnownScene(scene))
            return;
        for (const SceneBinding& b : out)
            if (b.token == token)
                return;
        out.push_back({token, scene});
    };
    for (const char c : text) {
        if (c == ',' || c == ';' || c == '|' || c == '\n') {
            flush();
            item.clear();
        } else {
            item.push_back(c);
        }
    }
    flush();
    return out;
}

// The default map, used when the user's text parses to nothing.
inline std::vector<SceneBinding> defaultSceneMap()
{
    return {{"b", kSceneBsod}, {"u", kSceneUpdate}, {"t", kSceneGcc},
            {"m", kSceneMatrix}, {"h", kSceneBlackhole}};
}

// The scene for a query's first word; "" when the word is not bound.
inline std::string resolveScene(const std::vector<SceneBinding>& map, const std::string& word)
{
    const std::string token = lower(trim(word));
    if (token.empty() || token.find_first_of(" \t") != std::string::npos)
        return {};
    for (const SceneBinding& b : map)
        if (b.token == token)
            return b.scene;
    return {};
}

// ---- the exit sequence ------------------------------------------------------

// The matcher's alphabet: what a keyboard gives without a dead key. Anything
// else (Enter, arrows, IME text) never reaches the buffer.
inline bool isSequenceChar(char c)
{
    return (c >= 'a' && c <= 'z') || (c >= '0' && c <= '9');
}

inline std::string normalizeSequence(const std::string& seq)
{
    std::string out;
    for (const char c : lower(seq))
        if (isSequenceChar(c))
            out.push_back(c);
    return out;
}

constexpr std::size_t kSequenceBufferKeep = 32;

// The rolling buffer's next state with one more key pressed, then whether it
// now ends with the sequence. The QML side keeps the buffer; this keeps the
// rule pure.
inline std::pair<std::string, bool> pushKey(std::string buffer, const std::string& sequence, char key)
{
    const char c = static_cast<char>(std::tolower(static_cast<unsigned char>(key)));
    if (!isSequenceChar(c))
        return {std::move(buffer), false};
    buffer.push_back(c);
    if (buffer.size() > kSequenceBufferKeep)
        buffer.erase(0, buffer.size() - kSequenceBufferKeep);
    const bool hit = !sequence.empty() && buffer.size() >= sequence.size()
        && buffer.compare(buffer.size() - sequence.size(), sequence.size(), sequence) == 0;
    return {std::move(buffer), hit};
}

// ---- the fake percent pacing ------------------------------------------------

// Piecewise-linear climb: fast at first, a stall where a real updater would
// sit, then a slow crawl. Never over 100; monotonic; loops never (the cover
// stays believable for minutes). The update scene stalls at 87 — the classic.
inline int coverPercent(int elapsedMs, const std::string& scene)
{
    if (elapsedMs <= 0)
        return 0;
    const double sec = elapsedMs / 1000.0;
    const bool update = scene == kSceneUpdate;
    const double stallAt = update ? 87.0 : 77.0;
    const double stallFrom = update ? 18.0 : 25.0;   // seconds when the stall begins
    const double stallUntil = update ? 55.0 : 70.0;  // seconds when the crawl resumes
    const double crawlTo = update ? 99.0 : 100.0;
    const double crawlUntil = update ? 240.0 : 300.0;
    double value;
    if (sec < 3.0)
        value = sec / 3.0 * 8.0;                          // 0-8 in the first beats
    else if (sec < stallFrom)
        value = 8.0 + (sec - 3.0) / (stallFrom - 3.0) * (stallAt - 8.0) * 0.75;
    else if (sec < stallUntil)
        value = 8.0 + (stallAt - 8.0) * 0.75
            + (sec - stallFrom) / (stallUntil - stallFrom) * (stallAt - 8.0) * 0.25;
    else if (sec < crawlUntil)
        value = stallAt + (sec - stallUntil) / (crawlUntil - stallUntil) * (crawlTo - stallAt);
    else
        value = crawlTo;
    if (value < 0.0)
        value = 0.0;
    if (value > 100.0)
        value = 100.0;
    return static_cast<int>(value);
}

// The seconds a scene keeps collecting before it claims a restart (the blue
// screen's "100% complete" beat). The QML side shows "restart" text after it.
inline int sceneRestartAfterSec(const std::string& scene)
{
    return scene == kSceneUpdate ? 240 : 300;
}

// ---- the gcc log ------------------------------------------------------------

// One pass of an endless build log: `total` translation units with the percent
// climbing, warnings in the real format, and a CMake regeneration at the end
// instead of a link step. The gcc scene streams pass after pass for as long as
// the cover is up, so a pass never reaches 100% and never links — a build that
// finishes would give the game away. Pass 0 opens with the invocation; later
// passes continue where the previous regeneration left off.
inline std::vector<std::string> gccLog(const std::string& project, unsigned seed, int pass = 0)
{
    struct Step {
        unsigned n;     // source-file pool index (mod the pool size)
        int line;       // warning line number, 0 = no warning
        const char* warning; // nullptr = none
    };
    static const char* const dirs[] = {"src/core", "src/net", "src/ui", "src/store", "tests"};
    static const char* const units[] = {"parser", "session", "transport", "cache", "layout",
                                        "profile", "worker", "codec", "index", "watcher",
                                        "stream", "history", "config", "notify", "repair"};
    static const char* const vars[] = {"retry", "count", "prev", "tmp", "len", "flags"};
    static const char* const warns[] = {"unused variable ‘%s’ [-Wunused-variable]",
                                        "comparison of integer expressions of different signedness: ‘int’ and ‘std::size_t’ {‘aka’ ‘unsigned long long’} [-Wsign-compare]",
                                        "‘%s’ may be used uninitialized [-Wmaybe-uninitialized]",
                                        "suggest parentheses around comparison in operand of ‘==’ [-Wparentheses]"};

    std::vector<std::string> lines;
    const std::string target = project.empty() ? "app" : project;
    unsigned state = seed ? seed : 1u;
    auto next = [&state](unsigned mod) {
        state = state * 1664525u + 1013904223u; // an LCG keeps this header-free
        return state % mod;
    };

    if (pass <= 0) {
        lines.push_back("$ ninja -C build");
        lines.push_back("ninja: Entering directory `build'");
        lines.push_back("[1/2] Regenerating build files (CMake)");
        lines.push_back("-- The CXX compiler identification is GNU 13.2.0");
        lines.push_back("-- Configuring done");
        lines.push_back("-- Generating done");
        lines.push_back("[2/2] Building all targets");
    }

    const int total = 64; // units per pass; the percent tops out just under 100
    for (int i = 0; i < total; ++i) {
        const std::string dir = dirs[next(5)];
        const std::string unit = units[next(15)];
        const std::string file = dir + "/" + unit + ".cpp";
        const int done = i + 1;
        lines.push_back("[" + std::to_string(done * 98 / total) + "%] "
                        + (next(4) == 0 ? "Building C object " : "Building CXX object ")
                        + "CMakeFiles/" + target + ".dir/" + file + ".o");
        if (const int warned = next(10); warned < 3) { // about three warnings per ten units
            char buf[160];
            const int line = 12 + static_cast<int>(next(180));
            std::snprintf(buf, sizeof buf, "%s:%d:%d: warning: ", file.c_str(), line, 5 + static_cast<int>(next(20)));
            std::string text = buf;
            const int kind = warned;
            if (kind == 0 || kind == 2)
                text += std::string(vars[next(6)]);
            // The signedness text has no placeholder; fill only the ones that do.
            std::string fmt = warns[kind];
            std::size_t pos = 0;
            while ((pos = fmt.find("%s", pos)) != std::string::npos) {
                fmt.replace(pos, 2, kind == 3 ? "==" : vars[next(6)]);
                pos += 2;
            }
            text += fmt;
            lines.push_back(text);
        }
    }
    // CMake noticed a changed list mid-build: the run restarts instead of
    // linking, and the next pass picks the count up from the bottom again.
    lines.push_back("[1/1] Re-running CMake...");
    lines.push_back("-- Configuring done");
    lines.push_back("-- Generating done");
    return lines;
}

} // namespace bosskey

#endif // BOSSKEY_LOGIC_H
