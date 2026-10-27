///////////////////////////////////////////////////////////////////////////////////////////////////////
// Copyright (C) 2026 The PakalEngine Open Source Project
// File: CharacterViewComponent.h
// Purpose: The animated model of a character: clips after its state and speed, what it holds in its
//          hands, the hurt flash and the boost glow. It reads the other components (and a
//          CharacterViewSource, the brain of an enemy) when the frame is drawn.
///////////////////////////////////////////////////////////////////////////////////////////////////////

#pragma once
#include "topdown/TopDownComponents.h"

#include <nlohmann/json_fwd.hpp>

#include <array>
#include <map>
#include <memory>
#include <string>
#include <utility>
#include <vector>

namespace Pakal
{
	class MeshComponent3D_Bgfx;
}

namespace Pakal::TopDown
{
	// The animated model of a character (player or zombie). Pose and color are worked out from the
	// other components when the frame is drawn.
	class CharacterViewComponent : public Component
	{
	public:
		struct Clips
		{
			std::vector<std::string> idle, move, attack, death;
			std::vector<std::string> fast; // moving quickly (berserk zombies run, the player sprints)
			std::vector<std::string> hit;  // played once when hurt
			// Knocked back: thrown down, then up again within the stun (a short stun plays the hit clip instead)
			std::vector<std::string> knockDown, getUp;
			std::vector<std::string> melee; // one swing of a melee weapon (the player, while a consumable lends one)
			// The player's gun: the upper body plays these over the legs (Look::overlayRoot)
			std::vector<std::string> fire;     // one fire cycle, looped while firing
			std::vector<std::string> reload;   // stretched over the reload time
			std::vector<std::string> walkFire; // the legs while walking and firing (slower than running)
			// With a melee weapon in hand: standing, moving, and a swing while moving
			std::vector<std::string> meleeIdle, meleeMove, meleeMoving;
			// Standing with a two-handed gun: a rest that holds it (the plain idle lets it hang from one hand, out of the
			// other's reach)
			std::vector<std::string> gunIdle;
			// The steps of a special attack by name (CharacterAction, from the brain): "attack", "prepare", "explode",
			// "backOff", "release", "spawn", "run", "pounce", "inAir", "windmill"
			std::map<std::string, std::vector<std::string>> actions;
		};

		// A model held in a hand: the bone it follows and where it sits there (meters, degrees)
		struct Held
		{
			std::string mesh;
			std::string bone;
			float offset[3] = { 0.0f, 0.0f, 0.0f };
			float euler[3] = { 0.0f, 0.0f, 0.0f };
			float scale = 1.0f;
			// Where the other hand takes it (in the weapon's model): a long gun is held with both hands always, a pistol while
			// firing (the look's 'reach' arm, by two-bone IK)
			bool hasGrip = false;
			float grip[3] = { 0.0f, 0.0f, 0.0f };
			bool twoHanded = false;

			// Where it sits in the bone's frame (column-major): its offset, then its rotations (Z, Y, X, degrees), then its scale
			std::array<float, 16> placement() const;
			// Where the other hand takes it, in the bone's frame (false: it has no grip)
			bool grip_in_bone(float out[3]) const;
		};

		// How a character looks and moves: its clips, size, color and what it holds
		struct Look
		{
			Clips clips;
			float height = 1.8f;
			std::string model;      // model file of this type of enemy, relative to the level ("" for the level's enemy model)
			std::vector<std::string> variants; // more models for the same type: each zombie gets one of them at random
			int pool = 0;           // copies of its model made at load (0: the level's enemyPool)
			float tint[3] = { 1.0f, 1.0f, 1.0f };
			float walkRef = 1.5f;   // meters per second one cycle of the move clip covers
			float runRef = 4.5f;    // ... of the fast clip
			float fastAbove = 3.2f; // speed (m/s) from which the fast clip is used
			std::string overlayRoot = "Abdomen"; // the bone from which up the fire and reload clips play
			bool hasHeld = false;
			Held held;
			std::vector<std::pair<int, Held>> weapons; // melee weapons by consumable id: what the hand holds while one is lent
			// The other arm (shoulder, elbow and the bone at the hand) that reaches for a weapon's grip; empty: none
			std::string reach[3];

			// A look from JSON (a level's "visuals" entry, a game's data) over 'defaults': what it leaves out keeps the default
			static Look from_json(const nlohmann::json& json, const Look& defaults);
		};

		CharacterViewComponent(std::unique_ptr<Pakal::MeshComponent3D_Bgfx> mesh, const Look& look);
		~CharacterViewComponent() override;

		// The model of what the character holds (made by the level); follows the Look's bone
		void set_held(std::unique_ptr<Pakal::MeshComponent3D_Bgfx> held);
		// The model of a melee weapon, shown instead of the gun while that consumable is active
		void add_weapon(int consumableId, std::unique_ptr<Pakal::MeshComponent3D_Bgfx> mesh, const Held& held);
		// The gun shown in the hand when nothing is lent: one of the added weapons (the player's equipped gun), 0 for the held one
		void set_base_weapon(int consumableId) { m_baseWeapon = consumableId; }

		// Places the model and picks its clip (called once per drawn frame). 'alpha' is how far the frame is between the
		// last simulation step and the next (Actor::drawn_position).
		void sync(float dt, float alpha = 1.0f);
		// Debug: plays this clip whatever the actor does ("" to go back to normal)
		void force_clip(const std::string& name) { m_forced = name; }
		std::unique_ptr<Pakal::MeshComponent3D_Bgfx> release_mesh();
		std::unique_ptr<Pakal::MeshComponent3D_Bgfx> release_held();
		// Which pool the model came from (zombies of different types use different models)
		void set_model_key(const std::string& key) { m_modelKey = key; }
		const std::string& model_key() const { return m_modelKey; }
		// The color of the type of zombie that uses this model next (pooled models change role)
		void set_tint(float r, float g, float b) { m_look.tint[0] = r; m_look.tint[1] = g; m_look.tint[2] = b; }
		float height() const { return m_look.height; }
		// Frozen (the player is down): the clip stops where it is
		void freeze(bool on) { m_frozen = on; }

	private:
		std::unique_ptr<Pakal::MeshComponent3D_Bgfx> m_mesh;
		std::unique_ptr<Pakal::MeshComponent3D_Bgfx> m_held;
		int m_baseWeapon = 0;
		std::map<int, std::unique_ptr<Pakal::MeshComponent3D_Bgfx>> m_weaponMeshes;
		std::map<int, Held> m_weaponHeld;   // how each of them sits in the hand (for the other hand's grip)
		float m_reachWeight = 0.0f;         // the other hand on the gun, blended in and out
		Look m_look;
		b2Vec2 m_lastPosition = b2Vec2(0.0f, 0.0f);
		bool m_moving = false;
		float m_speed = 0.0f;         // meters per second, smoothed
		float m_yawTarget = 0.0f;
		bool m_yawKnown = false;
		bool m_wasAttacking = false;
		bool m_wasHurt = false;
		int m_hurtClipsSeen = 0;      // an enemy's hurt clips already played
		bool m_frozen = false;
		int m_knockStep = 0;          // 1 thrown down, 2 getting up
		float m_knockClipLeft = 0.0f;
		bool m_wasKnocked = false;
		bool m_wasSwinging = false;
		float m_attackClipSpeed = 1.0f;
		float m_hitTimer = 0.0f;      // seconds left of the hit clip
		float m_hitCooldown = 0.0f;
		float m_boostClock = 0.0f;   // the glow of a boost pulses with it
		bool m_wasSwingClip = false;  // a melee swing clip is playing
		float m_swingClipSeconds = 0.0f;
		int m_actionSerial = 0;      // the special attack step being shown
		float m_actionSpeed = 1.0f;
		std::string m_forced;
		std::string m_modelKey;
	};
}
