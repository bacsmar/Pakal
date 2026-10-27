// Integration check against a generated level; no game assets are required to run it.
#include "box2D/PhysicsWorld2D.h"
#include "gameplay/NavGrid2D.h"
#include <nlohmann/json.hpp>
#include <cmath>
#include <cstdio>
#include <fstream>
#include <vector>

using namespace Pakal;
using Json = nlohmann::json;

int main(int argc, char** argv)
{
    if (argc != 2) { std::fprintf(stderr, "Usage: LevelReachabilityTests <level.json>\n"); return 2; }
    int failures = 0;
    auto check = [&](bool ok, const char* message) {
        std::printf("[%s] %s\n", ok ? "PASS" : "FAIL", message);
        std::fflush(stdout);
        if (!ok) ++failures;
    };
    std::ifstream input(argv[1]);
    Json doc;
    input >> doc;
    PhysicsWorld2D world({0, 0});
    const float units = doc.at("units_per_meter").get<float>();
    b2Vec2 low(1e9f, 1e9f), high(-1e9f, -1e9f);
    auto extend = [&](b2Vec2 p) {
        low.x = std::min(low.x, p.x); low.y = std::min(low.y, p.y);
        high.x = std::max(high.x, p.x); high.y = std::max(high.y, p.y);
    };
    for (const auto& wall : doc.at("walls")) {
        b2Vec2 p(wall.at("x").get<float>() / units, wall.at("y").get<float>() / units);
        if (wall.at("shape") == "rect")
            world.add_static_box(p, {wall.at("w").get<float>() / units / 2, wall.at("h").get<float>() / units / 2});
        else if (wall.at("shape") == "circle")
            world.add_static_circle(p, wall.at("d").get<float>() / units / 2);
        extend(p);
    }
    for (const auto& segment : doc.at("collision").at("segments")) {
        if (segment.at("layer") != "solid" && segment.at("layer") != "player") continue;
        std::vector<b2Vec2> points;
        for (const auto& p : segment.at("points")) {
            points.emplace_back(p[0].get<float>() / units, p[1].get<float>() / units);
            extend(points.back());
        }
        if (segment.value("filled", false))
            world.add_static_polygon(points, segment.at("layer") == "player" ? 0x0008 : 0x0001,
                segment.at("layer") == "player" ? 0x0002 : 0xFFFF);
        else world.add_static_chain(points, true);
    }
    const auto& at = doc.at("script").at("points").at("45036");
    b2Vec2 spawn(at[0].get<float>() / units, at[1].get<float>() / units);
    check(std::abs(spawn.x - 22.01995f) < 0.001f && std::abs(spawn.y - 37.30218f) < 0.001f,
        "DINER_SPAWN matches authored level coordinates");
    check(world.distance_to_static(spawn) >= 0.3f, "DINER_SPAWN clears static collision");
    NavGrid2D nav(0.5f);
    nav.build(world, low - b2Vec2(10, 10), high + b2Vec2(10, 10), 0.3f, 0x0001);
    check(world.point_in_static({22.02f,45.0f}), "diner interior is a solid polygon");
    check(world.raycast(spawn, {22.02f,45.0f}, 0x0001).hit, "diner wall blocks shots from sidewalk");
    int reachable = 0;
    for (const auto& spawner : doc.at("spawners")) {
        std::vector<b2Vec2> route;
        b2Vec2 p(spawner.at("x").get<float>() / units, spawner.at("y").get<float>() / units);
        if (nav.find_path(spawn, p, route)) ++reachable;
    }
    std::printf("DINER_SPAWN: %d reachable spawners\n", reachable);
    std::fflush(stdout);
    check(reachable > 0, "DINER_SPAWN reaches enemy spawners with original closed contours");
    return failures == 0 ? 0 : 1;
}
