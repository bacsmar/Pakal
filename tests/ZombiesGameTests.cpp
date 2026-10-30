// Headless checks of the Zombies simulation's timing against the original game's numbers (game clock, no window)
#include "ZombiesGame.h"
#include "Components.h"
#include "topdown/kits/shooter/WeaponComponent.h"
#include "topdown/Height.h"
#include "GameRules.h"
#include "box2D/PhysicsWorld2D.h"
#include "gameplay/NavGrid2D.h"

#include <cmath>
#include <cstdio>

using namespace Zombies;

namespace
{
	int failures = 0;
	void check(bool ok, const char* message)
	{
		std::printf("[%s] %s\n", ok ? "PASS" : "FAIL", message);
		std::fflush(stdout);
		if (!ok) ++failures;
	}

	// The dual Glock: FireRate 350 ms, trigger times 50 and 430 ms of its 600 ms fire clip
	GunRules glock()
	{
		GunRules gun;
		gun.fireRateMs = 350.0f;
		gun.triggers = { 50.0f / 600.0f, 430.0f / 600.0f };
		gun.magazine = 100;
		gun.rounds = 1000;
		return gun;
	}

	// Holds the trigger of a gun for 'seconds' and counts the shots
	int shots_in(const GunRules& gun, float seconds, std::vector<b2Vec2>* directions = nullptr)
	{
		Pakal::PhysicsWorld2D world({ 0, 0 });
		Pakal::NavGrid2D nav(0.5f);
		GameRules rules;
		ZombiesGame game(world, nav, rules, 7);
		int shots = 0;
		game.events.subscribe<ShotEvent>([&shots](const ShotEvent&) { ++shots; });
		Actor& shooter = game.add_actor(ActorKind::Character, "shooter", game.survivors);
		WeaponComponent& weapon = shooter.add<WeaponComponent>(gun);
		weapon.set_trigger(true);
		const float dt = 1.0f / 60.0f;
		for (float t = 0.0f; t < seconds - 1e-4f; t += dt)
		{
			game.step(dt, PlayerInput());
			if (directions)
			{
				for (const auto& bullet : game.bullets()) directions->push_back(bullet.direction);
			}
		}
		return shots;
	}

	// The player at the origin (with a lot of health and no invincibility after a hit) and one zombie of a type, on open
	// ground. More enemy types can be added to 'rules' before; spawnEnemy brings them in.
	struct Arena
	{
		// on the heap: a Box2D world carries a large stack allocator, and main holds many arenas
		std::unique_ptr<Pakal::PhysicsWorld2D> physics = std::make_unique<Pakal::PhysicsWorld2D>(b2Vec2(0, 0));
		Pakal::PhysicsWorld2D& world = *physics;
		Pakal::NavGrid2D nav{ 0.5f };
		GameRules rules;
		std::unique_ptr<ZombiesGame> game;
		Actor* player = nullptr;
		Actor* zombie = nullptr;
		Arena(const EnemyRules& enemy, const b2Vec2& at, const std::string& type = "25001", const GameRules* more = nullptr)
		{
			if (more) rules = *more;
			rules.enemies[type] = enemy;
			nav.build(world, b2Vec2(-30.0f, -30.0f), b2Vec2(30.0f, 30.0f), 0.3f, 0x0001);
			game = std::make_unique<ZombiesGame>(world, nav, rules, 3);
			game->spawnEnemy = [this](const std::string& kind, const b2Vec2& where) { return add(kind, where).id; };
			player = &game->add_actor(ActorKind::Character, "player", game->survivors);
			player->add<HealthComponent>(1.0e6f, 0.0f);
			game->add_player(*player);
			zombie = &add(type, at);
		}
		Actor& add(const std::string& type, const b2Vec2& at)
		{
			Actor& actor = game->add_actor(ActorKind::Character, type, game->undead);
			actor.position = at;
			actor.add<EnemyBrainComponent>(*game, type, at, actor.id);
			return actor;
		}
		float taken() const { return 1.0e6f - player->get<HealthComponent>()->current(); }
		int enemies_of(const std::string& type) const
		{
			int count = 0;
			for (const auto& actor : game->actors())
			{
				const EnemyBrainComponent* brain = actor->get<EnemyBrainComponent>();
				if (brain && brain->type() == type && !brain->is_dead()) ++count;
			}
			return count;
		}
	};

	// The special enemies, with the original's numbers (Zombies_GameObjects_Library)
	EnemyRules boomer()
	{
		EnemyRules e;
		e.attack = EnemyAttack::Boomer;
		e.speed = 400.0f;
		e.health = 4.0f;
		e.attackDamage = 15.0f;
		e.attackRange = 200.0f;
		e.attackRadius = 500.0f;
		e.attackDelayMs = 1600.0f;
		e.attackClipMs = 2866.7f;
		e.attackIntervalMinMs = 500.0f;
		e.attackIntervalMaxMs = 1000.0f;
		e.shotSpeed = 1000.0f;
		e.shotDamage = 10.0f;
		e.collisionSize = 250.0f;
		return e;
	}

	EnemyRules tourist()
	{
		EnemyRules e;
		e.attack = EnemyAttack::Bomber;
		e.speed = 525.0f;
		e.health = 0.25f;
		e.attackDamage = 15.0f;
		e.attackRange = 300.0f;
		e.attackRadius = 400.0f;
		e.attackDelayMs = 500.0f;
		e.prepareMs = 300.0f;
		e.explodeClipMs = 466.7f;
		e.attackIntervalMinMs = 1000.0f;
		e.attackIntervalMaxMs = 2000.0f;
		e.fadeOut = false;
		return e;
	}

	EnemyRules dog()
	{
		EnemyRules e;
		e.attack = EnemyAttack::Dog;
		e.speed = 500.0f;
		e.health = 0.25f;
		e.attackDamage = 7.0f;
		e.attackRange = 0.0f;
		e.attackIntervalMinMs = 0.0f;
		e.attackIntervalMaxMs = 0.0f;
		e.runSpeed = 500.0f;
		e.pounceRadius = 100.0f;
		e.spawnClipMs = 133.3f;
		e.pounceClipMs = 400.0f;
		e.collisionSize = 80.0f;
		return e;
	}

	EnemyRules dog_walker()
	{
		EnemyRules e;
		e.attack = EnemyAttack::DogWalker;
		e.speed = 425.0f;
		e.health = 1.25f;
		e.attackRadius = 600.0f;
		e.attackRange = 600.0f;
		e.attackIntervalMinMs = 0.0f;
		e.attackIntervalMaxMs = 0.0f;
		e.backOffRadius = 350.0f;
		e.backwardSpeed = 500.0f;
		e.spawnType = "25013";
		e.spawnIntervalMs = 1000.0f;
		e.spawnDelayMs = 1000.0f;
		e.spawnClipMs = 1666.7f;
		return e;
	}

	EnemyRules zombinator()
	{
		EnemyRules e;
		e.attack = EnemyAttack::Constant;
		e.speed = 75.0f;
		e.health = 5.0f;
		e.mass = 10.0f;
		e.attackDamage = 20.0f;
		e.attackRange = 250.0f;
		e.attackRadius = 600.0f;
		e.attackIntervalMinMs = 0.0f;
		e.attackIntervalMaxMs = 0.0f;
		e.runSpeed = 75.0f;
		e.attackClipMs = 1666.7f;
		return e;
	}
}

int main()
{
	// Cycles start at 0, 0.35 and 0.70 s; each lets out two shots (29 and 251 ms in): 6 in one second
	const int shots = shots_in(glock(), 1.0f);
	std::printf("  glock: %d shots in 1 s\n", shots);
	check(shots == 6, "the dual Glock fires two shots per 350 ms cycle, as the original");

	GunRules single = glock();
	single.triggers = { 0.0f };
	check(shots_in(single, 1.0f) == 3, "a gun with one trigger time fires once per cycle");

	// A shotgun: 4 pellets over +-35 degrees, evenly (the outer ones at the edges)
	GunRules shotgun = single;
	shotgun.pellets = 4;
	shotgun.spreadDegrees = 70.0f;
	std::vector<b2Vec2> directions;
	shots_in(shotgun, 1.0f / 60.0f, &directions);
	bool even = directions.size() == 4;
	if (even)
	{
		// facing is +y; the angles from it should be -35, -11.67, 11.67, 35
		const float expected[4] = { -35.0f, -35.0f / 3.0f, 35.0f / 3.0f, 35.0f };
		for (int i = 0; i < 4; ++i)
		{
			const float angle = std::atan2(-directions[i].x, directions[i].y) * 57.29578f;
			even = even && std::fabs(std::fabs(angle) - std::fabs(expected[i])) < 0.5f;
		}
	}
	check(even, "shotgun pellets spread evenly from one edge of the angle to the other");

	// ZombieBusiness: attack clip 1.333 s (the hit 0.5 s into it), then a wait of 0.5-0.75 s: one hit every 1.83-2.08 s
	{
		EnemyRules business;
		business.attackClipMs = 1333.3f;
		business.attackDelayMs = 500.0f;
		business.attackIntervalMinMs = 500.0f;
		business.attackIntervalMaxMs = 750.0f;
		business.attackDamage = 1.0f;
		business.hurtClipMs = 866.7f;

		const float dt = 1.0f / 60.0f;

		Arena fight(business, b2Vec2(1.0f, 0.0f));
		std::vector<float> hitTimes;
		float last = 0.0f;
		for (float t = 0.0f; t < 10.0f; t += dt)
		{
			fight.game->step(dt, PlayerInput());
			if (fight.taken() > last + 0.5f) { hitTimes.push_back(t); last = fight.taken(); }
		}
		bool cycle = hitTimes.size() >= 4;
		for (size_t i = 1; i < hitTimes.size(); ++i)
		{
			const float gap = hitTimes[i] - hitTimes[i - 1];
			cycle = cycle && gap > 1.78f && gap < 2.13f;
		}
		std::printf("  business: %zu hits in 10 s, first at %.2f s\n", hitTimes.size(), hitTimes.empty() ? -1.0f : hitTimes[0]);
		check(cycle, "a ZombieBusiness hits once per 1.83-2.08 s (attack clip, then the wait)");

		// Hurt by a bullet right away: no attack while the hurt clip plays (0.867 s), so the first hit comes later
		Arena hurt(business, b2Vec2(1.0f, 0.0f));
		hurt.game->step(dt, PlayerInput());
		hurt.game->damage(*hurt.zombie, 0.1f);
		float firstHit = -1.0f;
		for (float t = dt; t < 4.0f && firstHit < 0.0f; t += dt)
		{
			hurt.game->step(dt, PlayerInput());
			if (hurt.taken() > 0.5f) firstHit = t;
		}
		std::printf("  hurt: first hit at %.2f s\n", firstHit);
		check(firstHit > 1.25f, "a hurt zombie starts no attack until its hurt clip ends");

		// Under steady fire it still attacks: its hurt clip (and the stun) comes at most once every 2 s
		business.health = 1000.0f;
		Arena steady(business, b2Vec2(1.0f, 0.0f));
		int steadyHits = 0;
		float lastTaken = 0.0f;
		for (float t = 0.0f; t < 6.0f; t += dt)
		{
			if (std::fmod(t, 0.1f) < dt) steady.game->damage(*steady.zombie, 0.01f);
			steady.game->step(dt, PlayerInput());
			if (steady.taken() > lastTaken + 0.5f) { ++steadyHits; lastTaken = steady.taken(); }
		}
		std::printf("  steady fire: %d hits in 6 s\n", steadyHits);
		check(steadyHits >= 2, "a zombie under steady fire is stunned only once every 2 s");
		business.health = 1.5f;

		// Knocked back (melee): mass 1 slides ~3.8 m away in ~0.3 s and stays stunned until 2 s
		Arena knock(business, b2Vec2(1.0f, 0.0f));
		knock.game->step(dt, PlayerInput());
		knock.game->damage(*knock.zombie, { .amount = 0.1f, .from = b2Vec2(0.0f, 0.0f), .knockback = true });
		float at05 = 0.0f, at19 = 0.0f;
		for (float t = dt; t < 1.95f; t += dt)
		{
			knock.game->step(dt, PlayerInput());
			if (at05 == 0.0f && t >= 0.5f) at05 = knock.zombie->position.x;
		}
		at19 = knock.zombie->position.x;
		std::printf("  knockback: x %.2f at 0.5 s, %.2f at 1.95 s\n", at05, at19);
		check(at05 > 4.0f && at05 < 5.6f, "a knockback slides a mass-1 zombie about 3.8 m");
		check(std::fabs(at19 - at05) < 0.05f, "a knocked-back zombie stays stunned until 2 s");
	}

	// Difficulty rows: each table has its own number of rows; past its last one the last is kept (ZombieBusiness's data)
	{
		EnemyRules business;
		business.speeds = { 200.0f };
		business.healths = { 1.5f, 5.25f, 9.0f };
		business.attackDamages = { 3.0f, 3.3f };
		business.berserkRows = { { 450.0f, 1.125f, 4.5f }, { 470.0f, 3.9f, 4.95f } };
		const EnemyRules::Row two = business.at(2, false);
		const EnemyRules::Row angry = business.at(5, true);
		check(two.speed == 200.0f && two.health == 9.0f && two.attackDamage == 3.3f, "difficulty 2 takes row 2 of each table, or its last row");
		check(angry.speed == 470.0f && angry.health == 3.9f, "berserk takes its own rows by difficulty");
	}

	// Melee: the trapezoid ahead (20 cm wide at the player, 400 cm at the reach) against bodies 150 cm wide; the battle axe hits a
	// circle all around. Bambino's kills fly (FLING_DEATH).
	{
		const float dt = 1.0f / 60.0f;
		EnemyRules business;
		business.health = 100.0f;
		auto hits = [&](const MeleeRules& melee, const b2Vec2& at)
		{
			Arena arena(business, at);
			arena.game->step(dt, PlayerInput());
			WeaponComponent& weapon = arena.player->add<WeaponComponent>(glock());
			weapon.swing(*arena.game, melee, 1.0f);
			return arena.zombie->get<EnemyBrainComponent>()->hurt_recently();
		};
		MeleeRules bambino;
		bambino.reach = 260.0f;
		bambino.damage = 2.5f;
		bambino.death = DeathKind::Fling;
		MeleeRules axe = bambino;
		axe.reach = 365.0f;
		axe.circle = true;
		// the player faces +y
		check(hits(bambino, b2Vec2(1.5f, 2.4f)), "a swing reaches wide at the end of its reach");
		check(!hits(bambino, b2Vec2(1.2f, 0.0f)), "a swing misses a zombie right beside the player");
		check(!hits(bambino, b2Vec2(0.0f, -1.5f)), "a swing misses behind");
		check(hits(axe, b2Vec2(0.0f, -3.0f)), "the battle axe hits all around");

		business.health = 1.0f;
		Arena fling(business, b2Vec2(0.0f, 1.5f));
		fling.game->step(dt, PlayerInput());
		fling.player->add<WeaponComponent>(glock()).swing(*fling.game, bambino, 1.0f);
		const EnemyBrainComponent* body = fling.zombie->get<EnemyBrainComponent>();
		float highest = 0.0f;
		for (float t = 0.0f; t < 3.5f; t += dt)
		{
			fling.game->step(dt, PlayerInput());
			highest = std::max(highest, body->lift());
		}
		std::printf("  fling: %.1f m high, landed %.1f m away\n", highest, fling.zombie->position.y);
		check(highest > 10.0f && body->lift() == 0.0f && fling.zombie->position.y > 9.0f, "a Bambino kill flies 11 m up and lands about 9 m away");
	}

	// The player down: everything else stands still (Level::FreezeObjects)
	{
		const float dt = 1.0f / 60.0f;
		EnemyRules business;
		Arena down(business, b2Vec2(-5.0f, 0.0f));
		down.game->step(dt, PlayerInput());
		down.game->damage(*down.player, 2.0e6f);
		const b2Vec2 before = down.zombie->position;
		for (float t = 0.0f; t < 1.0f; t += dt) down.game->step(dt, PlayerInput());
		check(down.game->frozen() && (down.zombie->position - before).Length() < 0.001f, "the zombies stand still while the player is down");
	}

	// The special enemies. Each starts facing +x, so it is placed on the -x side, looking at the player.
	{
		const float dt = 1.0f / 60.0f;

		// Boomer 1.5 m away: DamageDelay 1.6 s into its attack the blast (15) and its forward shot (10) hit; four shots leave
		Arena boom(boomer(), b2Vec2(-1.5f, 0.0f), "25016");
		float blastAt = -1.0f;
		size_t shotsOut = 0;
		for (float t = 0.0f; t < 1.75f; t += dt)
		{
			boom.game->step(dt, PlayerInput());
			if (blastAt < 0.0f && boom.taken() > 1.0f) { blastAt = t; shotsOut = boom.game->bullets().size(); }
		}
		std::printf("  boomer: hit at %.2f s, %zu shots out, %.1f taken\n", blastAt, shotsOut, boom.taken());
		check(blastAt > 1.55f && blastAt < 1.7f, "a boomer's blast lands 1.6 s into its attack");
		check(shotsOut >= 3 && std::fabs(boom.taken() - 25.0f) < 0.01f, "a boomer's blast (15) and its shot ahead (10) hit a player next to it");

		// Boomer 4 m away: inside AttackRadius 5 m it attacks from there; the blast (2 m) misses, the shot ahead hits
		Arena far(boomer(), b2Vec2(-4.0f, 0.0f), "25016");
		for (float t = 0.0f; t < 2.2f; t += dt) far.game->step(dt, PlayerInput());
		std::printf("  boomer at 4 m: %.1f taken, at x %.2f\n", far.taken(), far.zombie->position.x);
		check(std::fabs(far.taken() - 10.0f) < 0.01f && far.zombie->position.x < -3.9f, "a boomer attacks from 5 m: only its shot reaches the player 4 m off");

		// Tourist 2 m away: prepare 0.3 s, then 0.5 s into the explode clip it blows up (15) and is gone
		Arena bomb(tourist(), b2Vec2(-2.0f, 0.0f), "25010");
		float boomAt = -1.0f;
		for (float t = 0.0f; t < 1.5f && boomAt < 0.0f; t += dt)
		{
			bomb.game->step(dt, PlayerInput());
			if (bomb.taken() > 1.0f) boomAt = t;
		}
		const EnemyBrainComponent* bomber = bomb.zombie->get<EnemyBrainComponent>();
		std::printf("  tourist: blew up at %.2f s, %.1f taken\n", boomAt, bomb.taken());
		check(boomAt > 0.75f && boomAt < 0.9f && std::fabs(bomb.taken() - 15.0f) < 0.01f, "a tourist blows up 0.8 s after it starts, hurting the player 15");
		check(bomber->is_dead() && bomber->burst() && bomb.game->kills == 0, "a tourist dies of its own blast, with no reward");

		// A token of a "collect" mission stays on the floor until the player walks over it, and counts
		{
			GameRules withToken;
			PickupRules token;
			token.type = "Token";
			token.value = 1.0f;
			token.radius = 130.0f;
			token.lifeTimeMs = -1.0f;
			withToken.pickups[9002] = token;
			Arena field(tourist(), b2Vec2(25.0f, 25.0f), "25010", &withToken);
			field.game->drop_pickup(9002, b2Vec2(5.0f, 0.0f));
			for (int i = 0; i < 600; ++i) field.game->step(dt, PlayerInput());   // 10 s: it does not go away by itself
			const int before = field.game->tokens;
			field.player->position = b2Vec2(4.5f, 0.0f);
			field.game->step(dt, PlayerInput());
			field.game->step(dt, PlayerInput());
			check(before == 0 && field.game->tokens == 1, "a token stays until taken, and taking it counts");
		}

		// A health pack dropped by a broken prop: on the floor until the player walks over it, then it heals like using it
		{
			GameRules withHealth;
			ConsumableRules pack;
			pack.id = 15020;
			pack.addHealth = 25.0f;
			withHealth.consumables[15020] = pack;
			PickupRules dropped;
			dropped.type = "Consumable";
			dropped.consumable = 15020;
			dropped.radius = 150.0f;
			dropped.lifeTimeMs = 15000.0f;
			withHealth.pickups[15020] = dropped;
			Arena field(tourist(), b2Vec2(25.0f, 25.0f), "25010", &withHealth);
			field.player->add<ConsumableComponent>();
			HealthComponent* health = field.player->get<HealthComponent>();
			health->damage(100.0f);
			const float hurt = health->current();
			field.game->drop_pickup(15020, b2Vec2(5.0f, 0.0f));
			field.game->step(dt, PlayerInput());
			const float untouched = health->current();
			field.player->position = b2Vec2(4.5f, 0.0f);
			field.game->step(dt, PlayerInput());
			field.game->step(dt, PlayerInput());
			check(untouched == hurt && std::fabs(health->current() - (hurt + 25.0f)) < 0.01f, "a dropped health pack heals 25 when taken");
		}

		// Money left on the floor: a zombie that sees it comes for it, takes it, runs off with it, and drops it when killed
		{
			GameRules withMoney;
			PickupRules coin;
			coin.type = "Money";
			coin.value = 5.0f;
			coin.radius = 100.0f;
			withMoney.pickups[9001] = coin;
			EnemyRules walker;
			walker.attackDamage = 1.0f;
			Arena field(walker, b2Vec2(-7.0f, 0.0f), "25001", &withMoney);
			field.player->position = b2Vec2(15.0f, 0.0f);   // far: the zombie is not busy swinging at it
			field.game->drop_pickup(9001, b2Vec2(-3.0f, 3.0f));
			auto coins_on_floor = [&]()
			{
				int count = 0;
				for (const auto& actor : field.game->actors())
				{
					const PickupComponent* pickup = actor->get<PickupComponent>();
					if (pickup && !actor->removed && pickup->rules().type == "Money") ++count;
				}
				return count;
			};
			EnemyBrainComponent* brain = field.zombie->get<EnemyBrainComponent>();
			float tookAt = -1.0f;
			for (float t = 0.0f; t < 12.0f && tookAt < 0.0f; t += dt)
			{
				field.game->step(dt, PlayerInput());
				if (brain->coins_carried() == 1) tookAt = t;
			}
			const float near = (field.zombie->position - field.player->position).Length();
			for (int i = 0; i < 90; ++i) field.game->step(dt, PlayerInput());
			const float later = (field.zombie->position - field.player->position).Length();
			std::printf("  coin: taken at %.2f s, %.1f m from the player, then %.1f m; %.1f taken from the player\n", tookAt, near, later, field.taken());
			check(tookAt >= 3.0f && coins_on_floor() == 0 && field.game->money == 0, "a zombie takes money left on the floor");
			check(later > near + 1.0f && field.taken() < 0.5f, "it runs off with the coin instead of attacking");
			field.game->damage(*field.zombie, 1.0e9f);
			field.game->step(dt, PlayerInput());
			const int dropped = coins_on_floor();
			field.player->position = field.zombie->position;
			field.game->step(dt, PlayerInput());
			check(dropped == 1 && field.game->money >= 5, "killed, it drops the coin and the player takes it");
		}

		// Defend: a barricade between the zombie and the player is broken first (then it walks through); the player standing
		// next to it puts it back up. The thing to defend draws the zombies while the player is away.
		{
			EnemyRules walker;
			walker.attackDamage = 10.0f;
			Arena field(walker, b2Vec2(-7.0f, 0.0f));
			PropRules wall;
			wall.health = 30.0f;
			wall.repairable = true;
			wall.repairSeconds = 2.0f;
			Actor& barricade = field.game->add_actor(ActorKind::Prop, "Barricade");
			barricade.position = b2Vec2(-3.0f, 0.0f);
			DestructibleComponent& planks = barricade.add<DestructibleComponent>(wall);
			planks.set_box(b2Vec2(-3.4f, -3.0f), b2Vec2(-2.6f, 3.0f));
			field.world.add_static_box(b2Vec2(-3.0f, 0.0f), b2Vec2(0.4f, 3.0f), field.game->layers.barricade, barricade.id);
			field.game->barricades.push_back(barricade.id);
			float brokeAt = -1.0f;
			for (float t = 0.0f; t < 20.0f && brokeAt < 0.0f; t += dt)
			{
				field.game->step(dt, PlayerInput());
				if (planks.is_broken()) brokeAt = t;
			}
			for (int i = 0; i < 180; ++i) field.game->step(dt, PlayerInput());
			const float through = field.zombie->position.x;
			std::printf("  barricade: broken at %.2f s, the zombie then at x %.1f, %.1f taken from the player\n", brokeAt, through, field.taken());
			check(brokeAt > 0.0f && through > -2.0f, "a zombie breaks the barricade in its way and walks through");
			field.game->damage(*field.zombie, 1.0e9f);
			field.player->position = b2Vec2(-2.0f, 0.5f);
			for (int i = 0; i < 90; ++i) field.game->step(dt, PlayerInput());
			const bool halfway = planks.is_broken();
			for (int i = 0; i < 60; ++i) field.game->step(dt, PlayerInput());
			check(!halfway && !planks.is_broken() && planks.repair_share() > 0.99f, "standing next to it, the player repairs the barricade");
		}
		{
			// a hit on what breaks shows on its view for the game's seconds, then it stands as it was
			struct Seen : PropView
			{
				float last = -1.0f; int hits = 0, states = 0;
				void show_state(bool, bool) override { ++states; }
				void show_hit(float left) override { last = left; ++hits; }
			};
			EnemyRules walker;
			Arena field(walker, b2Vec2(-30.0f, 0.0f));
			PropRules can;
			can.health = 100.0f;
			Actor& thing = field.game->add_actor(ActorKind::Prop, "Can");
			auto seen = std::make_unique<Seen>();
			Seen* view = seen.get();
			PropComponent& prop = thing.add<PropComponent>(std::move(seen));
			DestructibleComponent& body = thing.add<DestructibleComponent>(can);
			body.damage(*field.game, 10.0f);
			const bool unseen = view->hits == 0;   // no seconds given: not shown
			prop.set_hit_seconds(0.2f);
			body.damage(*field.game, 10.0f);
			const int statesBefore = view->states;
			field.game->step(dt, PlayerInput());
			const float early = view->last;
			for (int i = 0; i < 30; ++i) field.game->step(dt, PlayerInput());
			check(unseen && early > 0.5f && early < 1.0f && view->last == 0.0f && view->states > statesBefore,
				"a hit on a breakable shows on its view, fades out and puts it back");
		}
		{
			EnemyRules walker;
			walker.attackDamage = 10.0f;
			Arena field(walker, b2Vec2(-7.0f, 0.0f));
			field.player->position = b2Vec2(15.0f, 0.0f);
			PropRules truck;
			truck.health = 1000.0f;
			truck.objective = true;
			Actor& goal = field.game->add_actor(ActorKind::Prop, "Truck");
			goal.position = b2Vec2(-6.0f, 6.0f);
			DestructibleComponent& body = goal.add<DestructibleComponent>(truck);
			body.set_box(b2Vec2(-7.0f, 5.0f), b2Vec2(-5.0f, 7.0f));
			field.world.add_static_box(b2Vec2(-6.0f, 6.0f), b2Vec2(1.0f, 1.0f), field.game->layers.wall, goal.id);
			field.game->objective = goal.id;
			for (int i = 0; i < 600; ++i) field.game->step(dt, PlayerInput());
			std::printf("  defend: the truck at %.0f of 1000, zombie id %d\n", body.health(), int(field.zombie->id));
			check(body.health() < 1000.0f && field.taken() < 0.5f, "zombies go for the thing to defend while the player is away");
		}

		// Tourist that starts at the edge of its 4 m: it keeps running at the player while it prepares, so its 3 m blast lands
		Arena edge(tourist(), b2Vec2(-3.9f, 0.0f), "25010");
		float edgeDistance = -1.0f;
		for (float t = 0.0f; t < 3.0f && edge.taken() < 1.0f; t += dt)
		{
			edge.game->step(dt, PlayerInput());
			if (edge.taken() > 1.0f) edgeDistance = std::fabs(edge.zombie->position.x);
		}
		std::printf("  tourist from 3.9 m: %.1f taken, blew up %.2f m away\n", edge.taken(), edgeDistance);
		check(std::fabs(edge.taken() - 15.0f) < 0.01f && edgeDistance >= 0.0f && edgeDistance < 2.0f, "a tourist closes in before it blows up");

		// Shot before it starts: it goes off at once, and a tourist next to it goes off in turn
		Arena chain(tourist(), b2Vec2(-12.0f, 0.0f), "25010");
		Actor& second = chain.add("25010", b2Vec2(-13.0f, 0.0f));
		chain.game->step(dt, PlayerInput());
		chain.game->damage(*chain.zombie, 1.0f);
		chain.game->step(dt, PlayerInput());
		std::printf("  tourists: %d killed\n", chain.game->kills);
		check(second.get<EnemyBrainComponent>()->is_dead() && chain.game->kills == 2, "a tourist shot down blows up at once and sets off the one next to it");

		// Dog 5 m away: spawn clip, a run at 5 m/s, a pounce within 1 m; touching the player it bites once (7) and is gone
		Arena bite(dog(), b2Vec2(-5.0f, 0.0f), "25013");
		float biteAt = -1.0f;
		for (float t = 0.0f; t < 3.0f && biteAt < 0.0f; t += dt)
		{
			bite.game->step(dt, PlayerInput());
			if (bite.taken() > 1.0f) biteAt = t;
		}
		std::printf("  dog: bit at %.2f s, %.1f taken\n", biteAt, bite.taken());
		check(biteAt > 0.7f && biteAt < 1.2f && std::fabs(bite.taken() - 7.0f) < 0.01f, "a dog runs 5 m in about a second and bites once (7)");
		check(bite.zombie->get<EnemyBrainComponent>()->is_dead(), "a dog is gone after its bite");

		// Dog walker 5 m away (between BackOffRadius and AttackRadius): its first dog comes out 1 s into the release
		GameRules withDog;
		withDog.enemies["25013"] = dog();
		Arena walker(dog_walker(), b2Vec2(-5.0f, 0.0f), "25012", &withDog);
		float dogAt = -1.0f;
		for (float t = 0.0f; t < 2.0f && dogAt < 0.0f; t += dt)
		{
			walker.game->step(dt, PlayerInput());
			if (walker.enemies_of("25013") > 0) dogAt = t;
		}
		std::printf("  dog walker: dog out at %.2f s\n", dogAt);
		check(dogAt > 0.95f && dogAt < 1.1f, "a dog walker's dog comes out 1 s into its release");

		// Dog walker 2 m away: it backs off at 5 m/s
		Arena back(dog_walker(), b2Vec2(-2.0f, 0.0f), "25012", &withDog);
		for (float t = 0.0f; t < 0.3f; t += dt) back.game->step(dt, PlayerInput());
		std::printf("  dog walker: backed to x %.2f in 0.3 s\n", back.zombie->position.x);
		check(back.zombie->position.x < -3.0f, "a dog walker backs off from a player closer than 3.5 m");

		// Zombinator 1.5 m away: it windmills and hurts 20 per second within 2.5 m (no invincibility here)
		Arena mill(zombinator(), b2Vec2(-1.5f, 0.0f), "25011");
		for (float t = 0.0f; t < 2.0f; t += dt) mill.game->step(dt, PlayerInput());
		std::printf("  zombinator: %.1f taken in 2 s\n", mill.taken());
		check(mill.taken() > 36.0f && mill.taken() < 41.0f, "a zombinator's windmill hurts 20 per second up close");
	}

	// The bear: it comes asleep (still, unhurt) for 5 s; awake it swats (25 within 4 m); having lost half its health it
	// rears up and its chain hits 45 within 5 m
	{
		const float dt = 1.0f / 60.0f;
		EnemyRules bear;
		bear.attack = EnemyAttack::Bear;
		bear.speed = 500.0f;
		bear.health = 100.0f;
		bear.mass = 100.0f;
		bear.attackDamage = 25.0f;
		bear.attackRange = 400.0f;
		bear.attackRadius = 500.0f;
		bear.attackDelayMs = 500.0f;
		bear.attackClipMs = 1333.4f;
		bear.attackIntervalMinMs = 0.0f;
		bear.attackIntervalMaxMs = 500.0f;
		bear.sleepMs = 5000.0f;
		bear.wakeClipMs = 1333.3f;
		bear.rearClipMs = 1600.0f;
		bear.abdomenMul = 1.3f;
		bear.chainClipMs = 2933.3f;
		Arena den(bear, b2Vec2(-3.0f, 0.0f), "25026");
		EnemyBrainComponent* brain = den.zombie->get<EnemyBrainComponent>();
		den.game->step(dt, PlayerInput());
		den.game->damage(*den.zombie, 60.0f);
		const float asleep = brain->health_share();
		float firstHit = -1.0f;
		for (float t = 0.0f; t < 10.0f && firstHit < 0.0f; t += dt)
		{
			den.game->step(dt, PlayerInput());
			if (den.taken() > 0.5f) firstHit = t;
		}
		std::printf("  bear: %.0f%% after a hit asleep, first swat at %.2f s (%.0f)\n", 100.0f * asleep, firstHit, den.taken());
		check(asleep > 0.99f && firstHit > 6.0f && std::fabs(den.taken() - 25.0f) < 0.01f, "the bear sleeps 5 s, unhurt, then swats for 25");
		den.game->damage(*den.zombie, 60.0f);
		const float before = den.taken();
		for (int i = 0; i < 600; ++i) den.game->step(dt, PlayerInput());
		const float chain = den.taken() - before;
		std::printf("  bear reared: %.0f taken in 10 s, health %.0f%%\n", chain, 100.0f * brain->health_share());
		check(chain >= 45.0f && std::fmod(chain, 45.0f) < 0.01f, "having lost half its health the bear rears up and its chain hits 45");
	}

	// A consumable whose effect (data) is an air strike calls a bombardment around the player
	{
		const float dt = 1.0f / 60.0f;
		EnemyRules walker;
		Arena field(walker, b2Vec2(-25.0f, 0.0f));
		ConsumableRules strike;
		strike.id = 777;
		strike.effect = "airstrike";
		ConsumableComponent& consumable = field.player->add<ConsumableComponent>();
		int blasts = 0;
		field.game->events.subscribe<ExplosionEvent>([&blasts](const ExplosionEvent&) { ++blasts; });
		consumable.apply(*field.game, strike);
		for (int i = 0; i < 300; ++i) field.game->step(dt, PlayerInput());
		std::printf("  air strike: %d blasts\n", blasts);
		check(blasts == 9, "a consumable with the airstrike effect brings nine blasts");
	}

	// Stats: (base + what modifiers add) times their factors; a modifier with a duration runs out by itself
	{
		Stats stats;
		stats.set_base("armor", 10.0f);
		const Stats::ModifierId plated = stats.add_modifier("armor", 5.0f, 2.0f);
		const Stats::ModifierId hasty = stats.add_modifier(Stat::speed, 0.0f, 1.5f, 1.0f);
		check(stats.value("armor") == 30.0f && stats.value(Stat::speed) == 1.5f && stats.value(Stat::damage) == 1.0f,
			"a stat is (base + added) times the factors; one never set is its fallback");
		stats.remove_modifier(plated);
		for (int i = 0; i < 61; ++i) stats.update(1.0f / 60.0f);
		check(stats.value("armor") == 10.0f && !stats.has_modifier(hasty) && stats.value(Stat::speed) == 1.0f,
			"a removed modifier and one whose time ran out change nothing");
	}

	// A hit goes through the target's resistance to its type and its defence, and is reported (DamageEvent)
	{
		const float dt = 1.0f / 60.0f;
		EnemyRules walker;
		Arena field(walker, b2Vec2(-25.0f, 0.0f));
		StatsComponent& stats = field.player->add<StatsComponent>();
		stats.resistances[DamageType::Attack] = 0.5f;
		stats.stats.add_modifier(Stat::defence, 0.0f, 2.0f);
		std::vector<DamageEvent> reported;
		field.game->events.subscribe<DamageEvent>([&reported](const DamageEvent& hit) { reported.push_back(hit); });
		field.game->damage(*field.player, { .amount = 8.0f, .type = DamageType::Attack, .source = field.zombie->id });
		field.game->damage(*field.player, { .amount = 8.0f, .type = DamageType::Bullet });
		check(reported.empty(), "hits are reported at the end of the step");
		field.game->step(dt, PlayerInput());
		check(std::fabs(field.taken() - 6.0f) < 1e-3f, "an attack is halved by the resistance and by defence 2 (2), a bullet only by the defence (4)");
		check(reported.size() == 2 && reported[0].target == field.player->id && reported[0].damage.source == field.zombie->id
			&& reported[0].damage.amount == 2.0f && reported[0].result.taken == 2.0f && !reported[0].result.killed,
			"each hit is reported with its target, its source and what it took");
	}

	// Factions: a faction is allied with itself, the ones set hostile are hostile both ways, the rest neutral
	{
		Factions factions;
		const int heroes = factions.add("heroes");
		const int bandits = factions.add("bandits");
		const int traders = factions.add("traders");
		factions.set(heroes, bandits, Relation::Hostile);
		factions.set(heroes, traders, Relation::Allied);
		check(factions.add("bandits") == bandits && factions.find("traders") == traders && factions.name(heroes) == "heroes",
			"factions are found by name");
		check(factions.hostile(bandits, heroes) && factions.relation(heroes, heroes) == Relation::Allied
			&& factions.relation(traders, heroes) == Relation::Allied && factions.relation(bandits, traders) == Relation::Neutral
			&& factions.relation(Factions::None, bandits) == Relation::Neutral, "and relations go both ways");

		EnemyRules walker;
		Arena field(walker, b2Vec2(-25.0f, 0.0f));
		Actor& survivor = field.game->add_actor(ActorKind::Character, "survivor", field.game->survivors);
		check(field.game->hostile(*field.player, *field.zombie) && !field.game->hostile(*field.player, survivor)
			&& field.game->faction_of(*field.zombie) == field.game->undead, "in Zombies the survivors and the zombies are hostile");
	}

	// Height: a jump leaves the floor and falls back under gravity; a swing misses what is above its reach
	{
		const float dt = 1.0f / 60.0f;
		EnemyRules walker;
		Arena field(walker, b2Vec2(0.0f, 1.0f));
		HeightComponent& jumper = field.player->add<HeightComponent>();
		check(jumper.jump(5.0f) && !jumper.jump(5.0f), "a character on the floor jumps; one in the air does not jump again");
		float peak = 0.0f;
		int steps = 0;
		do
		{
			field.game->step(dt, PlayerInput());
			peak = std::max(peak, jumper.height);
			++steps;
		} while (jumper.airborne() && steps < 200);
		std::printf("  jump: peak %.2f m, %d steps in the air\n", peak, steps);
		check(!jumper.airborne() && jumper.height == 0.0f && std::fabs(peak - 0.625f) < 0.1f && steps > 20 && steps < 40,
			"it rises about v^2/2g (0.62 m at 5 m/s) and lands about 2v/g later (0.5 s)");

		HeightComponent& flying = field.zombie->add<HeightComponent>();
		flying.height = 3.0f;
		MeleeRules bat;
		WeaponComponent& weapon = field.player->add<WeaponComponent>(glock());
		EnemyBrainComponent* brain = field.zombie->get<EnemyBrainComponent>();
		weapon.swing(*field.game, bat, 1.0f);
		const float afterHigh = brain->health_share();
		flying.height = 0.0f;
		weapon.swing(*field.game, bat, 1.0f);
		check(afterHigh == 1.0f && brain->health_share() < 1.0f, "a swing misses a zombie 3 m up and hits it on the floor");
	}

	// A bullet flies at its height: over a zombie 3 m up, into it on the floor
	{
		const float dt = 1.0f / 60.0f;
		EnemyRules walker;
		Arena field(walker, b2Vec2(5.0f, 0.0f));
		HeightComponent& flying = field.zombie->add<HeightComponent>();
		flying.height = 3.0f;
		flying.gravity = 0.0f;   // it stays up there
		EnemyBrainComponent* brain = field.zombie->get<EnemyBrainComponent>();
		field.game->fire_bullet(b2Vec2(0.0f, 0.0f), b2Vec2(1.0f, 0.0f), 20.0f, 0.1f, 10.0f, 1.0f, false, field.game->survivors, 1.0f);
		for (int i = 0; i < 40; ++i) field.game->step(dt, PlayerInput());
		const float afterHigh = brain->health_share();
		flying.height = 0.0f;
		field.game->fire_bullet(b2Vec2(0.0f, 0.0f), b2Vec2(1.0f, 0.0f), 20.0f, 0.1f, 10.0f, 1.0f, false, field.game->survivors, 1.0f);
		for (int i = 0; i < 40; ++i) field.game->step(dt, PlayerInput());
		check(afterHigh == 1.0f && brain->health_share() < 1.0f, "a bullet passes under a zombie 3 m up and hits it on the floor");
	}

	// A boost is a set of modifiers of the player's stats while it lasts
	{
		const float dt = 1.0f / 60.0f;
		EnemyRules walker;
		Arena field(walker, b2Vec2(-25.0f, 0.0f));
		field.player->add<StatsComponent>();
		ConsumableRules boost;
		boost.id = 778;
		boost.timerMs = 500.0f;
		boost.damageFactor = 2.0f;
		boost.defenceFactor = 4.0f;
		ConsumableComponent& consumable = field.player->add<ConsumableComponent>();
		consumable.apply(*field.game, boost);
		const float during = stat_of(*field.player, Stat::damage);
		field.game->damage(*field.player, 8.0f);
		const float takenDuring = field.taken();
		for (int i = 0; i < 40; ++i) field.game->step(dt, PlayerInput());
		check(during == 2.0f && takenDuring == 2.0f && stat_of(*field.player, Stat::damage) == 1.0f && !consumable.is_active(),
			"a damage and defence boost doubles the damage dealt and quarters the damage taken, until it runs out");
	}

	std::printf(failures == 0 ? "ALL PASSED\n" : "%d FAILED\n", failures);
	std::fflush(stdout);
	return failures == 0 ? 0 : 1;
}
