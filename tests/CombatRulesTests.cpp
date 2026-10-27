#include "topdown/EnemyAI2D.h"
#include "gameplay/NavGrid2D.h"
#include "box2D/CharacterController2D.h"
#include <cstdio>

int main()
{
    int failures = 0;
    auto check = [&](bool ok, const char* message) {
        std::printf("[%s] %s\n", ok ? "PASS" : "FAIL", message);
        std::fflush(stdout);
        if (!ok) ++failures;
    };
    // A shot is a ray cast against walls (0x0001) and enemies (0x0004)
    const uint16_t shotMask = 0x0005;
    auto shoot = [&](Pakal::PhysicsWorld2D& on) { return on.raycast({0, 0}, {30, 0}, shotMask); };
    Pakal::PhysicsWorld2D world({0, 0});
    // Enemies as a game declares them: their bodies on layer 0x0004, only walls (0x0001) block their sight
    Pakal::TopDown::EnemyConfig config;
    config.bodyCategory = 0x0004;
    config.sightBlockers = 0x0001;
    Pakal::TopDown::EnemyAI2D front(world, {2, 0}, config, 1), back(world, {4, 0}, config, 2);
    auto hit = shoot(world);
    check(hit.hit && hit.userData == 1, "shot hits nearest living enemy");
    front.apply_damage(100);
    hit = shoot(world);
    check(hit.hit && hit.userData == 2, "corpse does not shield living enemy");
    world.add_static_box({1, 0}, {0.1f, 1});
    hit = shoot(world);
    check(hit.hit && hit.staticBody && hit.userData == 0, "wall blocks shot before enemy");
    {
        Pakal::PhysicsWorld2D filters({0, 0});
        filters.add_static_polygon({{1,-2},{1.2f,-2},{1.2f,2},{1,2}}, 0x0008, 0x0002);
        Pakal::CharacterController2D player(filters.native(), {0,-0.75f}, 0.2f, 0x0002);
        Pakal::CharacterController2D enemy(filters.native(), {0,0.75f}, 0.2f, 0x0004);
        for (int i = 0; i < 120; ++i) {
            player.set_move({1,0}, 2); enemy.set_move({1,0}, 2);
            player.update(); enemy.update(); filters.step(1.0f / 60.0f);
        }
        check(player.get_position().x < 1 && enemy.get_position().x > 3,
            "player-only barrier blocks player while allowing enemies through");
        check(!filters.raycast({0,0},{2,0},shotMask).hit,
            "player-only barrier does not intercept weapon ray");
        check(filters.distance_to_static({1.1f,0}, 0x0001) > 1
            && filters.distance_to_static({1.1f,0}, 0x0008) == 0,
            "navigation obstacle mask excludes player-only barrier");
    }
    {
        Pakal::PhysicsWorld2D chaseWorld({0, 0});
        chaseWorld.add_static_box({0,0}, {0.2f,1.2f});
        Pakal::NavGrid2D nav(0.25f);
        nav.build(chaseWorld, {-5,-5}, {5,5}, 0.3f);
        Pakal::TopDown::EnemyConfig pursuerConfig = config;
        pursuerConfig.pursueTarget = true;
        Pakal::TopDown::EnemyAI2D pursuer(chaseWorld, {-2,0}, pursuerConfig, 3);
        float damage = 0;
        for (int i = 0; i < 600; ++i) {
            damage += pursuer.update(1.0f / 60.0f, {2,0}, nav, chaseWorld);
            chaseWorld.step(1.0f / 60.0f);
        }
        check(pursuer.get_position().x > 1 && damage > 0,
            "ordered enemy navigates around occluding wall and reaches player");
        Pakal::TopDown::EnemyConfig blockedConfig = config;
        blockedConfig.pursueTarget = true;
        blockedConfig.attackRange = 10;
        Pakal::TopDown::EnemyAI2D blocked(chaseWorld, {-2,0}, blockedConfig, 4);
        check(blocked.update(1.0f / 60.0f, {2,0}, nav, chaseWorld) == 0,
            "enemy never deals melee damage through wall");
    }
    return failures ? 1 : 0;
}
