///////////////////////////////////////////////////////////////////////////////////////////////////////
// Copyright (C) 2026 The PakalEngine Open Source Project
// File: WeaponComponent.h
// Purpose: The Shooter kit's weapon: a gun (magazine, reload, fire cycles, pellets and their spread,
//          two muzzles) or the melee weapon a consumable lends. The reload and the swing are actions
//          (ActionTimeline); the fire cycle is the gun's own: shots at the trigger times of each cycle,
//          chained while the trigger is held.
///////////////////////////////////////////////////////////////////////////////////////////////////////

#pragma once
#include "topdown/Action.h"
#include "topdown/TopDownComponents.h"

namespace Pakal::TopDown
{
	// The player's weapon: the gun of the rules, or the melee weapon a consumable lends
	class WeaponComponent : public Component, public PlayerWeapon, public WeaponPose
	{
	public:
		explicit WeaponComponent(const GunRules& gun);

		void set_trigger(bool pulled) override { m_trigger = pulled; }
		void update(TopDownGame& game, float dt) override;

		// The rounds of the gun in hand: the level's gun, or the firearm a consumable lends
		int magazine() const { return m_firearm ? m_fireMagazine : m_magazine; }
		int rounds() const { return m_firearm ? m_fireRounds : m_rounds; }
		const GunRules& gun() const { return m_firearm ? *m_firearm : m_gun; }
		bool reloading() const override { return m_reload.busy(); }
		bool firing() const override { return m_fireClipTimer > 0.0f; }   // for the firing pose
		bool swinging() const override { return m_swingTimer > 0.0f; }
		// 1 right after a shot, back to 0 in a moment: the body kicks back with it
		float recoil() const override { return m_recoil; }
		float reload_seconds() const override { return gun().reloadMs / 1000.0f; }
		float cycle_seconds() const override { return gun().fireRateMs / 1000.0f; }
		// The player's speed with this weapon (or the melee weapon a consumable lends), in meters per second, times its
		// speed stat
		float run_speed() const override;
		// One swing of a melee weapon: it hits the characters hostile to the player and the props its shape touches
		void swing(TopDownGame& game, const MeleeRules& melee, float damageFactor);

	private:
		void fire_gun(TopDownGame& game, float damageFactor);

		GunRules m_gun;
		const GunRules* m_firearm = nullptr;   // the firearm a consumable lends now, with its own rounds
		int m_fireMagazine = 0;
		int m_fireRounds = 0;
		bool m_trigger = false;
		int m_magazine;
		int m_rounds;
		// The reload: it winds up for the reload time, and the rounds go in as it becomes active
		ActionTimeline m_reload;
		// A melee weapon's swing: it hits as it starts, and cannot swing again for the weapon's attack rate
		ActionTimeline m_melee;
		float m_cycle = -1.0f;        // seconds into the gun's fire cycle (negative: not firing)
		size_t m_nextTrigger = 0;     // the next shot of the cycle
		float m_fireClipTimer = 0.0f;
		float m_swingTimer = 0.0f;
		float m_recoil = 0.0f;
		bool m_leftMuzzle = false;
	};
}
