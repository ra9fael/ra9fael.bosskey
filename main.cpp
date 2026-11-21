// main.cpp - WinShun resident plugin: Boss Key.
//
// The SDK owns stdio and the JSON-RPC framing (the serve loop runs on a worker
// thread of BossKeyApp.dll, see qmlapp/qmlmain.cpp). This file only maps
// queries to scene rows and the Enter action to a fullscreen cover; the window
// lifecycle lives in qmlapp/BossQml.cpp. Queries never touch the screen; the
// cover is shown only from an action.
//
// Code and comments are English; user-visible strings follow the host locale.
#include "winshun.h"
#include "logic.h"

#include "qmlapp/BossQml.h"

namespace {

struct Settings {
    std::vector<bosskey::SceneBinding> map;
    std::string exitSequence = "deadbeef";
    int autoHideSec = 0;
    std::string gccProject = "winshun-core";
    bool allScreens = false;
};

Settings readSettings(const winshun::Json& s)
{
    Settings st;
    const auto text = [&](const char* key, const char* fallback) -> std::string {
        const winshun::Json& v = s[key];
        if (v.isString())
            return v.toString();
        return fallback;
    };
    st.map = bosskey::parseSceneMap(text(
        "sceneMap", "bsod=bsod,update=update,gcc=gcc,matrix=matrix,blackhole=blackhole"));
    st.exitSequence = bosskey::normalizeSequence(text("exitSequence", "deadbeef"));
    if (st.exitSequence.empty())
        st.exitSequence = "deadbeef";
    const winshun::Json& autoHide = s["autoHideSec"];
    if (autoHide.isNumber())
        st.autoHideSec = static_cast<int>(autoHide.toInt());
    else if (autoHide.isString())
        st.autoHideSec = std::atoi(autoHide.toString().c_str());
    if (st.autoHideSec < 0)
        st.autoHideSec = 0;
    st.gccProject = bosskey::trim(text("gccProject", "winshun-core"));
    st.allScreens = bosskey::lower(text("displayAll", "mouse")) == "all";
    return st;
}

std::string tr(bool zh, const char* chinese, const char* english)
{
    return zh ? chinese : english;
}

// The scene's display name. The three ids are the protocol of the QML loader.
std::string sceneTitle(bool zh, const std::string& scene)
{
    if (scene == bosskey::kSceneBsod)
        return tr(zh, "蓝屏伪装", "Bluescreen cover");
    if (scene == bosskey::kSceneUpdate)
        return tr(zh, "系统更新伪装", "Windows Update cover");
    if (scene == bosskey::kSceneGcc)
        return tr(zh, "gcc 编译伪装", "gcc build cover");
    if (scene == bosskey::kSceneMatrix)
        return tr(zh, "黑客帝国代码雨", "Matrix digital rain");
    if (scene == bosskey::kSceneBlackhole)
        return tr(zh, "黑洞吸积盘", "Black hole accretion disk");
    return scene;
}

winshun::Item makeSceneRow(bool zh,
                           const std::string& token,
                           const std::string& scene,
                           const std::string& exitSequence,
                           int autoHideSec)
{
    // The trigger leads the row: the list says what to type, not only what the
    // scene looks like.
    std::string subtitle = "boss " + token + " · "
        + tr(zh, "Enter 全屏伪装 · 键入 ", "Enter to cover · type ")
        + exitSequence + tr(zh, " 退出", " to exit");
    if (autoHideSec > 0) {
        const std::string secs = std::to_string(autoHideSec);
        subtitle += zh ? " · " + secs + " 秒后自动消失"
                       : " · auto-dismiss in " + secs + "s";
    }
    winshun::Item row("cover:" + token, sceneTitle(zh, scene), subtitle);
    const char* color = scene == bosskey::kSceneBlackhole ? "#D97706"
                      : scene == bosskey::kSceneMatrix ? "#16A34A"
                      : scene == bosskey::kSceneGcc ? "#4EC9B0"
                      : scene == bosskey::kSceneUpdate ? "#2F6FED"
                      : "#0078D7";
    row.withIcon(std::string("color:") + color);
    row.withScore(100);
    row.withData(winshun::Json::Object {
        {"scene", scene},
        {"token", token},
    });
    row.withActions({{"cover", tr(zh, "全屏伪装", "Cover the screen"), "Enter"}});
    return row;
}

class BossKeyPlugin final : public winshun::Plugin {
    std::string name() const override { return "ra9fael.bosskey"; }

    winshun::Items query(const winshun::Query& q) override
    {
        const bool zh = chinese();
        const Settings st = readSettings(settings());
        std::vector<bosskey::SceneBinding> map = st.map;
        if (map.empty())
            map = bosskey::defaultSceneMap();

        // The first word after the keyword picks the scene; nothing lists them.
        std::string word = bosskey::trim(q.text);
        if (const auto space = word.find_first_of(" \t"); space != std::string::npos)
            word = word.substr(0, space);

        winshun::Items items;
        if (word.empty()) {
            for (const bosskey::SceneBinding& b : map)
                items.push_back(makeSceneRow(zh, b.token, b.scene, st.exitSequence, st.autoHideSec));
            return items;
        }
        const std::string scene = bosskey::resolveScene(map, word);
        if (scene.empty()) {
            std::string known;
            for (const bosskey::SceneBinding& b : map) {
                if (!known.empty())
                    known += " / ";
                known += b.token;
            }
            winshun::Item hint("unknown", tr(zh, "未知场景：", "Unknown scene: ") + word,
                               tr(zh, "可用子词：", "Available tokens: ") + known
                                   + tr(zh, "（插件设置里可改）", " (configurable in settings)"));
            hint.withIcon("color:#E8A33D");
            hint.withScore(50);
            return {hint};
        }
        return {makeSceneRow(zh, word, scene, st.exitSequence, st.autoHideSec)};
    }

    winshun::Effects action(const winshun::ActionRequest& a) override
    {
        const bool zh = chinese();
        const std::string scene = a.item["data"]["scene"].toString();
        if (a.action != "cover" || !bosskey::isKnownScene(scene))
            return {winshun::effect::keep()};
        const Settings st = readSettings(settings());
        bosskey::CoverSpec spec;
        spec.scene = scene;
        spec.exitSequence = st.exitSequence;
        spec.autoHideSec = st.autoHideSec;
        spec.gccProject = st.gccProject;
        spec.allScreens = st.allScreens;
        spec.zh = zh;
        std::string error;
        if (!bosskey::showCover(spec, error)) {
            winshun::debug("ra9fael.bosskey: cover failed: " + error);
            return {winshun::effect::notify(tr(zh, "无法打开伪装窗口：", "Could not open the cover: ") + error),
                    winshun::effect::hide()};
        }
        return {winshun::effect::notify(tr(zh, "已全屏伪装，键入 ", "Cover on; type ")
                                            + st.exitSequence + tr(zh, " 返回", " to return")),
                winshun::effect::hide()};
    }
};

} // namespace

namespace bosskey {

// The DLL's worker thread runs this (qmlapp/qmlmain.cpp); the GUI thread runs
// the cover windows.
int runServeLoop()
{
    BossKeyPlugin plugin;
    const int rc = plugin.serve();
    winshun::debug("ra9fael.bosskey: serve() returned " + std::to_string(rc));
    return rc;
}

} // namespace bosskey
