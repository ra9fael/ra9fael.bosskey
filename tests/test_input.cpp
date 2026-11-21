#include "qmlapp/input/KeyPolicy.h"
#include "qmlapp/input/GuardProtocol.h"
#include <cstdio>

namespace {
int failures = 0;
void expect(bool condition, const char* what)
{
    if (!condition) {
        std::printf("FAIL: %s\n", what);
        ++failures;
    }
}

bool type(bosskey::KeyPolicy& policy, const std::string& text)
{
    bool exit = false;
    for (char c : text) {
        const unsigned key = c >= 'a' && c <= 'z' ? c - 'a' + 'A' : c;
        auto down = policy.key(key, true);
        expect(down.swallow, "sequence key-down is swallowed");
        exit = exit || down.exit;
        expect(policy.key(key, false).swallow, "sequence key-up is swallowed");
    }
    return exit;
}
}

int main()
{
    using bosskey::KeyPolicy;
    {
        KeyPolicy policy("deadbeef");
        expect(!type(policy, "xdeadbee"), "a partial sequence does not exit");
        expect(type(policy, "f"), "a complete sequence exits without a focused window");
        expect(!type(policy, "deadbeef"), "only one exit event per cover");
        expect(policy.drained(), "all swallowed keys were released");
    }
    {
        KeyPolicy policy("aab");
        expect(type(policy, "aaaab"), "overlapping prefixes match");
        KeyPolicy empty("");
        expect(!type(empty, "abc"), "empty sequences never exit");
    }
    {
        KeyPolicy policy("ab");
        expect(!policy.key('A', true).exit, "first letter does not exit");
        expect(!policy.key('A', true).exit, "auto-repeat does not advance sequence");
        policy.key('A', false);
        expect(policy.key('B', true).exit, "second distinct key completes sequence");
        policy.drain();
        expect(!policy.drained(), "a held exit key needs draining");
        expect(policy.key('B', true).swallow, "draining suppresses held-key repeats");
        expect(!policy.key('C', true).swallow, "draining does not capture new typing");
        expect(policy.key('B', false).swallow, "draining consumes the matching key-up");
        expect(policy.drained(), "release completes draining");
    }
    {
        KeyPolicy policy("r");
        expect(policy.key(VK_LWIN, true).swallow, "Windows key is swallowed");
        auto result = policy.key('R', true);
        expect(result.swallow && !result.exit, "Win+R is suppressed, not matched");
        policy.drain();
        expect(policy.key(VK_LWIN, false).swallow, "drain consumes Win key-up");
        expect(policy.key('R', false).swallow, "drain consumes shortcut key-up");
        expect(policy.drained(), "Windows shortcut leaves no swallowed keys");
        expect(!policy.key(VK_LCONTROL, false).swallow, "unmatched modifier release is passed through");
    }
    {
        KeyPolicy policy("1");
        expect(policy.key(VK_NUMPAD1, true).exit, "numpad digits match");
        KeyPolicy ctrl("a");
        ctrl.initialDown(VK_LCONTROL, true);
        expect(!ctrl.key('A', true).exit, "initially-held Ctrl prevents sequence matching");
        expect(!ctrl.key(VK_LCONTROL, false).swallow, "initial modifier release is not swallowed");
        KeyPolicy shortcuts("deadbeef");
        shortcuts.key(VK_LCONTROL, true);
        shortcuts.key(VK_LSHIFT, true);
        expect(shortcuts.key(VK_ESCAPE, true).swallow, "Ctrl+Shift+Esc is swallowed");
        KeyPolicy alt("deadbeef");
        expect(alt.key(VK_TAB, true, true).swallow, "Alt+Tab honors the hook Alt flag");
        expect(!alt.key(999, true).swallow, "unknown virtual keys pass through");
    }
    using namespace bosskey::guard;
    expect(validSequence(L"deadbeef012"), "guard accepts normalized exit sequences");
    expect(!validSequence(L"") && !validSequence(L"ABC") && !validSequence(L"abc --owner 1"),
           "guard rejects empty, non-normalized and command-like sequences");
    expect(!validSequence(std::wstring(65, L'a')), "guard bounds sequence arguments");
    expect(validSession(L"12345678-1234-1234-1234-123456789abc"), "guard accepts session IDs");
    expect(!validSession(L"..\\another-pipe"), "guard rejects arbitrary pipe paths");
    // INVALID_HANDLE_VALUE is the current process's pseudo handle, so it is no
    // test of a failure path: a null handle is neither openable nor a pseudo one.
    expect(!elevated(nullptr), "a null process handle cannot enable enhanced mode");
    std::printf("Input policy: %s\n", failures ? "FAILED" : "passed");
    return failures ? 1 : 0;
}
