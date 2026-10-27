#include "../examples/zombies_module/ZombieScript.h"
#include "topdown/CameraPan.h"
#include "LevelDocument.h"
#include <cmath>
#include <cstdio>
#include <string>
#include <vector>

int main(int argc, char** argv)
{
    int failures = 0, pans = 0, returns = 0;
    bool controls = true, complete = false, killedAll = false, berserk = false, gift = false;
    std::vector<int> requested;
    auto check = [&](bool ok, const char* message) {
        std::printf("[%s] %s\n", ok ? "PASS" : "FAIL", message);
        std::fflush(stdout);
        if (!ok) ++failures;
    };
    Zombies::ZombieScript::Host host;
    host.log = [](const std::string& message) { std::printf("[script] %s\n", message.c_str()); };
    host.enableControls = [&](bool enabled) { controls = enabled; };
    host.showDialog = [](const std::string&, float) {};
    host.cameraPan = [&](int a, const std::string& offset, int b, const std::string&, float seconds) {
        check(a == 45038 && b == 45038 && seconds == 7 && offset == "300 -650 900", "camera receives original anchors, offset and duration");
        ++pans;
    };
    host.cameraReturnToPlayer = [&]() { ++returns; };
    host.setEnemySpawn = [&](int, int count, float) { requested.push_back(count); };
    host.killAllZombies = [&]() { killedAll = true; };
    host.doneKillAll = [&]() { complete = true; };
    host.showObject = [&](int id, bool shown) { if (id == 45035 && shown) gift = true; };
    host.enableRandomObject = [&](const std::vector<int>& ids, bool enabled) { if (enabled) for (int id : ids) if (id == 45035) gift = true; };
    host.enableBerserk = [&](bool on) { berserk = on; };
    Zombies::ZombieScript script(host);
    script.start("intro", "EnableControls(false); CameraSetManualPan(45038, '300 -650 900', 45038, '-350 -650 500', 7); ShowAutoDialog('intro', 7000); CameraReturnToPlayer(); EnableControls(true)", false);
    script.update(0);
    check(!controls && pans == 1 && returns == 0, "intro suspends during dialog and keeps controls disabled");
    script.update(6999);
    check(!controls && script.is_running("intro"), "dialog does not finish early");
    script.update(7000);
    check(controls && returns == 1 && !script.is_running("intro"), "dialog ends, camera returns and controls resume");
    script.start("cancel", "Wait(100); SetGameVariable('cancelled',1)", false);
    script.update(7000);
    script.stop("cancel");
    script.update(8000);
    check(script.get_variable("cancelled") == 0, "stopped coroutine never resumes");

    // FadeOut / FadeIn hold the script for their time; the clapper and the music do not; EnableSkip lets skip() cut the waits short
    {
        std::vector<std::string> fades, clappers, music;
        Zombies::ZombieScript::Host cut;
        cut.log = [](const std::string& message) { std::printf("[script] %s\n", message.c_str()); };
        cut.fade = [&](bool toColor, float ms, const std::string& color) { fades.push_back(std::string(toColor ? "out " : "in ") + std::to_string(int(ms)) + " " + color); };
        cut.showClapper = [&](const std::string& text, float ms) { clappers.push_back(text + " " + std::to_string(int(ms))); };
        cut.playMusic = [&](const std::string& name) { music.push_back(name); };
        cut.showDialog = [](const std::string&, float) {};
        Zombies::ZombieScript scene(cut);
        scene.start("cut", "FadeOut(1000, 'ffff0000'); PlayMusic('m_theme'); ShowClapper(7, 'SCENE'); EnableSkip(true); ShowAutoDialog('x', 5000); EnableSkip(false); FadeIn(500, 'ffffffff'); SetGameVariable('after', 1)", false);
        scene.update(0);
        check(fades.size() == 1 && fades[0] == "out 1000 ffff0000" && scene.get_variable("after") == 0, "FadeOut starts the fade and holds the script");
        scene.update(999);
        check(music.empty(), "the script waits for the fade to end");
        scene.update(1000);
        check(music.size() == 1 && music[0] == "m_theme" && clappers.size() == 1 && clappers[0] == "SCENE 7000" && scene.skippable(),
              "music and the clapper do not hold the script; EnableSkip makes the cutscene skippable");
        scene.update(3000);
        check(scene.get_variable("after") == 0, "the dialog still holds the script");
        scene.skip();
        scene.update(3001);
        check(!scene.skippable() && fades.size() == 2 && fades[1] == "in 500 ffffffff" && scene.get_variable("after") == 0,
              "skipping goes through the dialog at once, EnableSkip(false) ends the skip and the next fade keeps its time");
        scene.update(3501);
        check(scene.get_variable("after") == 1, "the script goes on after the fade");
    }

    // ReelObject hands the host the ids the reel can hide in
    {
        std::vector<int> reelIds;
        Zombies::ZombieScript::Host reelHost;
        reelHost.reelObject = [&](const std::vector<int>& ids) { reelIds = ids; };
        Zombies::ZombieScript reelScript(reelHost);
        reelScript.start("reel", "ReelObject(215, 218, 424)", false);
        reelScript.update(0);
        check(reelIds == std::vector<int>({ 215, 218, 424 }), "ReelObject passes the ids it can hide in");
    }

    Pakal::TopDown::CameraPan pan;
    pan.fromEye = {0, 10, 0}; pan.toEye = {10, 20, 5};
    pan.fromTarget = {0, 0, 0}; pan.toTarget = {0, 0, 10};
    pan.startedMs = 1000; pan.durationMs = 7000;
    Pakal::TopDown::CameraPan::Point eye, target;
    pan.sample(4500, eye, target);
    check(std::abs(eye[0] - 5) < 1e-5 && target[2] == 5, "camera interpolates eye and focus at midpoint");
    pan.sample(20000, eye, target);
    check(eye == pan.toEye && target == pan.toTarget, "camera holds last pose after pan until explicitly returned");
    pan.durationMs = 0;
    pan.sample(1000, eye, target);
    check(eye == pan.toEye, "zero-duration pan has no division by zero");

    if (argc > 1) {
        Pakal::LevelDocument doc;
        std::string error;
        if (!doc.load(argv[1], error)) { std::fprintf(stderr, "%s\n", error.c_str()); return 1; }
        requested.clear(); pans = returns = 0;
        Zombies::ZombieScript real(host);
        const auto objective = doc.script.at("objectives").at("5804");
        real.start("OnActivate", objective.at("onActivate").get<std::string>(), false);
        double now = 0;
        for (; now <= 6000; now += 100) real.update(now);
        check(!controls && requested.empty(), "real Survival intro has no early spawn orders");
        for (; now <= 30000; now += 100) real.update(now);
        check(!real.is_running("OnActivate") && controls && pans == 1 && returns == 1, "real intro finishes with camera returned and controls enabled");
        check(requested.size() == 2 && requested[0] == 4 && requested[1] == 4, "real script requests initial 4/4 wave");
        real.start("Run", objective.at("run").get<std::string>(), true);
        auto advance = [&](int kills, int enemies, double milliseconds) {
            real.set_variable("killCount", kills); real.set_variable("enemyCount", enemies);
            const double end = now + milliseconds;
            do { real.update(now); now += 100; } while (now <= end);
        };
        advance(8, 1, 0);
        check(real.get_variable("A") == 1, "8 kills stop first wave");
        advance(8, 0, 4900);
        check(real.get_variable("A") == 1, "next wave waits full five seconds after clear");
        advance(8, 0, 200);
        check(real.get_variable("A") == 2 && requested[requested.size()-2] == 8 && requested.back() == 5, "second wave requests 8/5");
        advance(15, 1, 0); advance(15, 0, 5200);
        check(real.get_variable("A") == 4 && requested[requested.size()-2] == 4 && requested.back() == 4, "15 kills then clear request 4/4");
        advance(25, 1, 0);
        check(real.get_variable("A") == 5 && gift, "25 kills enable original gift");
        advance(25, 0, 5200);
        check(real.get_variable("A") == 6 && berserk && requested[requested.size()-2] == 10 && requested.back() == 10, "final wave requests 10/10 and berserk");
        // Berserk dialog also blocks Run for 3500 ms before the win check.
        advance(35, 1, 4000);
        check(killedAll && complete, "35 kills trigger source KillAllZombies and DoneKillAll");
    }
    std::fflush(stdout);
    return failures ? 1 : 0;
}
