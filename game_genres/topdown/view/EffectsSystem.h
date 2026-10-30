///////////////////////////////////////////////////////////////////////////////////////////////////////
// Copyright (C) 2026 The PakalEngine Open Source Project
// File: EffectsSystem.h
// Purpose: The look of shooting: the bullet streak (the game's bullet texture), the muzzle flash and the
//          shell casing (the game's own models), and the sparks or blood where a bullet lands. The game
//          reports shots and impacts; this draws them. Models are made on the graphics thread.
//          Also the particles of explosions and of props that break: debris, dust, glass, gushing water,
//          a burning car's fire and smoke (BgfxGraphicsSystem::draw_particle). And what the fights leave on the floor
//          (draw_ground_spot): the muzzle's light flickering around the shooter, blood pools and splats that stay, bodies
//          blown to pieces, and the dirt of what climbs out of it.
///////////////////////////////////////////////////////////////////////////////////////////////////////

#pragma once
#include "RandomStreams.h"
#include "topdown/TopDownGame.h"

#include <functional>
#include <map>
#include <memory>
#include <string>
#include <vector>

namespace Pakal
{
	class BgfxGraphicsSystem;
	class MeshComponent3D_Bgfx;
}

namespace Pakal::TopDown
{
	class EffectsSystem
	{
	public:
		// Makes a (shared) instance of a model file, or null when it cannot
		using MeshFactory = std::function<std::unique_ptr<Pakal::MeshComponent3D_Bgfx>(const std::string& file)>;

		// A missing file just leaves that effect out. Call on the graphics thread. 'seed': its random stream ("fx").
		EffectsSystem(Pakal::BgfxGraphicsSystem* graphics, const MeshFactory& make, const std::string& bullet,
			const std::string& muzzle, const std::string& shell, uint32_t seed);
		~EffectsSystem();

		// What a game tunes: which prop effect is which (by its name: a word it contains, or the whole name), how long the
		// lasting ones run and how thick they are, and the blood's color
		enum class PropEffect { Water, Fire, Glass, Debris };
		struct PropEffectName { std::string word; PropEffect effect; bool whole = false; };
		struct Settings
		{
			std::vector<PropEffectName> propEffects{ { "Water", PropEffect::Water }, { "Fire", PropEffect::Fire },
				{ "Glass", PropEffect::Glass }, { "Break", PropEffect::Debris, true } };
			float waterSeconds = 60.0f;     // broken water gushes this long
			float fireSeconds = 40.0f;      // a burnt prop burns this long
			float waterRate = 90.0f;        // particles per second
			float fireRate = 75.0f;         // ... per prop about 4 m wide
			float smokeRate = 10.0f;
			float fountainRate = 55.0f;     // a fountain's drops per second
			float blood[3] = { 0.42f, 0.02f, 0.03f };
		};
		void set_settings(const Settings& settings) { m_settings = settings; }
		const Settings& settings() const { return m_settings; }

		void on_shot(const b2Vec2& muzzle, const b2Vec2& direction);
		// The same, from where the gun really is (render space: the hand of the drawn character)
		void on_shot_at(const float at[3], const b2Vec2& direction);
		void on_impact(const b2Vec2& at, const b2Vec2& direction, ImpactKind kind);
		// An explosion of 'radius' meters: a big flash and flying sparks
		void on_blast(const b2Vec2& at, float radius);
		// A ground pound: a ring of dust that runs out to the radius, and a short pale flash on the floor
		void on_shockwave(const b2Vec2& at, float radius);
		// A prop's effect (PropEffectEvent): water, glass, fire and smoke, or "Break" (debris and
		// dust). 'size' is about how wide the prop is (meters).
		void on_prop_effect(const b2Vec2& at, const std::string& effect, float size);
		// A body goes down at 'at', killed from 'from': a pool of blood under it; 'burst' blows it to pieces (chunks, a mist
		// and splats thrown away from the blow), 'big' throws more
		void on_body_down(const b2Vec2& at, const b2Vec2& from, bool burst, bool big);
		// Something climbs out of the floor: dirt and dust, or the grey grit of a manhole
		void on_climb_out(const b2Vec2& at, bool manhole);
		// A fountain that runs for good: water rising from 'height' meters and falling back into a basin 'radius' wide
		void add_fountain(const b2Vec2& at, float radius, float height);
		// A fire that burns until it is put out (a mission's: a car on fire), known by 'key' (not 0): on lights it (or
		// keeps it), off lets it die down over a few seconds
		void set_fire(int key, const b2Vec2& at, float size, bool on);
		// The player is hit: blood from the chest and a drop on the floor
		void on_player_hit(const b2Vec2& at);
		// A power-up taken at 'at' (render color 'rgb'): a burst of light climbing around the one who took it and a ring on the
		// floor
		void on_power_up(const b2Vec2& at, const float rgb[3]);
		// A camera's flash at 'at' facing 'toward' (unit): a white burst at chest height, a cone of light ahead on the floor
		void on_camera_flash(const b2Vec2& at, const b2Vec2& toward);
		// The floor's height at a point (meters; none: 0 everywhere): where what is thrown lands
		void set_floor(std::function<float(const b2Vec2&)> floor) { m_floor = std::move(floor); }
		// Whether a point of the floor is water (none: nowhere): no marks there, and what is thrown sinks in it
		void set_water(std::function<bool(const b2Vec2&)> water) { m_water = std::move(water); }
		// Pieces to throw (throw_pieces): 'copies' of each model file kept ready (graphics thread), as one set; returns its id
		int stock_pieces(const std::vector<std::string>& files, int copies);
		// How a body's pieces fly: 'count' of one set, 'speed' meters per second outward and 'up' upward (each picked
		// between its two), within 'spread' radians about the way away from the blow (2 pi: every way), from 'start' meters
		// up; 'gravity' pulls them down, each landing keeps 'bounce' of their speed until it can no longer lift them; they
		// turn 'spin' radians a second about the vertical (or 'tumble' every way), lie 'lie' seconds settled and shrink away
		struct PieceThrow
		{
			int set = 0;
			int count = 4;
			float speed[2] = { 3.0f, 6.0f }, up[2] = { 3.0f, 6.0f }, spin[2] = { 3.0f, 4.0f }, start[2] = { 0.3f, 0.6f };
			float spread = 6.2831853f, gravity = 9.8f, bounce = 0.4f, lie = 1.0f, scale = 1.0f;
			bool tumble = false;
		};
		void throw_pieces(const b2Vec2& at, const b2Vec2& from, const PieceThrow& how);
		// Something falls in the water at 'at' (the water's surface 'height' meters up), 'size' about how big: drops thrown up
		// and a ripple running out over the surface
		void on_splash(const b2Vec2& at, float height, float size);
		// A firework bursting 'height' meters over 'at' in color 'rgb': a ball of glowing stars that falls and fades, and its
		// light on the floor below
		void on_firework(const b2Vec2& at, float height, const float rgb[3]);
		// While a power-up lasts, call every frame: a glow under the one who has it and sparks rising from it (dt: this frame)
		void aura(const b2Vec2& at, const float rgb[3], float dt);
		// The enemies' blood: its color (a game's enemies may bleed green), or none at all (an option: no blood, no stains, and
		// a body that bursts goes up in dust). The player's blood is always red, and also goes with 'shown'.
		void set_blood(bool shown, float red, float green, float blue);
		// The sprites of the graphics' particle sheet (BgfxGraphicsSystem::set_particle_sprites), by name: its first cell
		// (from 1) and how many (a flipbook's frames, or variants picked at random). Known names: fire (a flipbook), smoke,
		// sparkle, ring, crack, splat, pool. Without them every particle is the soft dot.
		void set_sprites(const std::map<std::string, std::pair<int, int>>& sprites) { m_sprites = sprites; }
		// The spent casings the shots throw: their model (empty: the one given at the start), its scale, how long each lies
		// on the floor (seconds) and how many can be out at once (the oldest is taken again past them)
		void set_casings(const std::string& model, float scale, float life, size_t count);

		// Moves and draws everything. Call once per drawn frame.
		void update(float dt, const std::vector<Bullet>& bullets);

		// Drops the models (graphics thread)
		void release();

	private:
		struct Flash
		{
			std::unique_ptr<Pakal::MeshComponent3D_Bgfx> mesh;
			float age = -1.0f;        // < 0: free
			float life = 0.0f;
			float scale = 1.0f;
			float x = 0.0f, y = 0.0f; // level meters
			float height = 1.0f;
			float yaw = 0.0f;
			float color[3] = { 1.0f, 1.0f, 1.0f };
		};

		struct Piece
		{
			std::unique_ptr<Pakal::MeshComponent3D_Bgfx> mesh;
			float age = -1.0f;   // < 0: free
			float life = 0.0f, scale = 1.0f;
			float x = 0.0f, y = 0.0f, h = 0.0f;
			float vx = 0.0f, vy = 0.0f, vh = 0.0f;
			float spin = 0.0f, turn = 0.0f, tilt = 0.0f;
			float gravity = 9.8f, bounce = 0.4f, lie = 1.0f;
			int set = 0;
			bool tumble = false;
			bool settled = false;   // it lies still: 'life' counts its last seconds
			bool sinking = false;   // it fell in the water
		};
		std::vector<Piece> m_pieces;
		int m_pieceSets = 0;
		float m_shotHeight = 1.0f;   // the height the player's last shot left its gun at (its bullets fly there)
		MeshFactory m_make;
		std::string m_shell;
		float m_casingScale, m_casingLife;
		size_t m_nextCasing = 0;
		std::function<float(const b2Vec2&)> m_floor;
		std::function<bool(const b2Vec2&)> m_water;

		struct Casing
		{
			std::unique_ptr<Pakal::MeshComponent3D_Bgfx> mesh;
			float age = -1.0f;
			float x = 0.0f, y = 0.0f, h = 0.0f;     // level meters, height
			float vx = 0.0f, vy = 0.0f, vh = 0.0f;
			float spin = 0.0f, turn = 0.0f;
			int bounces = 0;
		};

		struct Spark
		{
			float x, y, h;
			float vx, vy, vh;
			float age, life;
			unsigned color;
		};

		// A soft particle: it flies (level meters, height), grows or shrinks from r0 to r1 and fades from a0 to a1 over its life
		struct Puff
		{
			float x, y, h;
			float vx, vy, vh;
			float age, life;
			float r0, r1;
			float a0, a1;
			float red, green, blue;
			float gravity;      // negative rises (fire, smoke)
			bool additive;
			bool bounce;        // a chunk bounces off the floor; the others stop on it
			int sprite = 0;     // a cell of the particle sheet (0: the soft dot); with 'frames' > 1 a flipbook over its life
			int frames = 1;
			float angle = 0.0f, spin = 0.0f;   // its turn (radians) and how fast it turns
		};

		// Something that keeps giving off particles for a while: gushing water, a fire
		struct Emitter
		{
			enum class Kind { Water, Fire, Fountain } kind;
			float x, y, size;
			float secondsLeft;
			float height = 0.0f;   // a fountain: where its water leaves (meters above the floor)
			float waterDue = 0.0f, fireDue = 0.0f, smokeDue = 0.0f;
			int key = 0;           // a fire a script lights and puts out (set_fire); 0: one that dies down by itself
		};

		// A mark lying on the floor: blood that stays (and fades at the end of its life), or a moment of light (additive)
		struct Stain
		{
			float x, y;
			float radius;
			float grow;        // seconds it takes to spread to its radius
			float age, life;
			float red, green, blue, alpha;
			bool light;
			int sprite = 0;    // a cell of the particle sheet (0: the soft disc), turned 'angle'
			float angle = 0.0f;
		};

		void start_flash(float x, float y, float height, float yaw, float scale, float life, float r, float g, float b);
		void add_stain(const Stain& stain);
		void splatter(float x, float y, const b2Vec2& away, int drops, float reach, float size, const float rgb[3]);
		static uint32_t abgr(const float rgb[3], float bright = 1.0f, float alpha = 1.0f);
		void add_puff(const Puff& puff);
		void burst_debris(float x, float y, float size, int count);
		void update_emitters(float dt);
		float random(float low, float high);
		// A cell of the named sprite (one of its variants at random), 0 when there is none; with 'frames', its first cell
		// and its frames (a flipbook)
		int sprite(const char* name, int* frames = nullptr);
		// The stain with a blood sprite ("splat", "pool") at a random turn, when there are sprites
		Stain shaped(Stain stain, const char* name);
		std::map<std::string, std::pair<int, int>> m_sprites;

		Pakal::BgfxGraphicsSystem* m_graphics;
		std::vector<std::unique_ptr<Pakal::MeshComponent3D_Bgfx>> m_bullets;
		std::vector<Flash> m_flashes;
		std::vector<Casing> m_casings;
		std::vector<Spark> m_sparks;
		std::vector<Puff> m_puffs;
		std::vector<Emitter> m_emitters;
		Settings m_settings;
		std::vector<Stain> m_stains;   // blood on the floor (the oldest go first past the most kept)
		std::vector<Stain> m_lights;   // the muzzle's light on the floor
		float m_auraClock = 0.0f, m_auraDue = 0.0f;
		bool m_bloodShown = true;
		float m_blood[3] = { 0.42f, 0.02f, 0.03f };   // the enemies' blood
		Pakal::RandomStream m_random;
	};
}
