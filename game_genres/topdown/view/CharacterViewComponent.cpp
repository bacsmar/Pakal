///////////////////////////////////////////////////////////////////////////////////////////////////////
// Copyright (C) 2026 The PakalEngine Open Source Project
// File: CharacterViewComponent.cpp
// Purpose: The animated model of a character - implementation
///////////////////////////////////////////////////////////////////////////////////////////////////////

#include "topdown/view/CharacterViewComponent.h"
#include "topdown/ActorOrientation.h"
#include "topdown/Height.h"

#include "bgfx/MeshComponent3D_Bgfx.h"

#include <nlohmann/json.hpp>

#include <algorithm>
#include <cmath>

namespace Pakal::TopDown
{
	namespace
	{
		// Plays the first clip of the list the model has, unless it is already playing
		void play_clip(Pakal::MeshComponent3D_Bgfx& mesh, const std::vector<std::string>& names, bool loop)
		{
			for (const auto& name : names)
			{
				const int index = mesh.find_animation(name);
				if (index >= 0)
				{
					if (mesh.get_current_animation() != index)
					{
						mesh.play_animation(index, loop);
					}
					return;
				}
			}
		}
	}

	namespace
	{
		const float kTurnSpeed = 6.2831853f;   // radians per second: the game's RotateSpeed of 360 degrees
		const float kMinFacingSpeed = 0.4f;    // slower than this (m/s) the body keeps its facing
		const float kHitCooldown = 0.6f;       // seconds between two hit reactions of the same body
		const float kBackpedalCos = 0.35f;     // walking more than ~110 degrees away from the aim is walking backwards
		const float kKnockDownAbove = 1.0f;    // a knockback stun this long or longer throws the body down

		float angle_difference(float target, float current)
		{
			float d = std::fmod(target - current + 3.14159265f, 6.2831853f);
			if (d < 0.0f) d += 6.2831853f;
			return d - 3.14159265f;
		}

		// Column-major 4x4 matrices (bgfx layout)
		using Mat4 = std::array<float, 16>;

		Mat4 multiply(const Mat4& a, const Mat4& b)
		{
			Mat4 out{};
			for (int c = 0; c < 4; ++c)
				for (int r = 0; r < 4; ++r)
				{
					float sum = 0.0f;
					for (int k = 0; k < 4; ++k) sum += a[k * 4 + r] * b[c * 4 + k];
					out[c * 4 + r] = sum;
				}
			return out;
		}

		// translate * Rz * Ry * Rx * scale, rotations in degrees
		Mat4 local_matrix(const float offset[3], const float euler[3], float scale)
		{
			const float k = 3.14159265f / 180.0f;
			const float cx = std::cos(euler[0] * k), sx = std::sin(euler[0] * k);
			const float cy = std::cos(euler[1] * k), sy = std::sin(euler[1] * k);
			const float cz = std::cos(euler[2] * k), sz = std::sin(euler[2] * k);
			const Mat4 rx{ 1, 0, 0, 0, 0, cx, sx, 0, 0, -sx, cx, 0, 0, 0, 0, 1 };
			const Mat4 ry{ cy, 0, -sy, 0, 0, 1, 0, 0, sy, 0, cy, 0, 0, 0, 0, 1 };
			const Mat4 rz{ cz, sz, 0, 0, -sz, cz, 0, 0, 0, 0, 1, 0, 0, 0, 0, 1 };
			Mat4 m = multiply(rz, multiply(ry, rx));
			for (int c = 0; c < 3; ++c)
				for (int r = 0; r < 3; ++r) m[c * 4 + r] *= scale;
			m[12] = offset[0];
			m[13] = offset[1];
			m[14] = offset[2];
			return m;
		}
	}

	namespace
	{
		std::vector<std::string> string_list(const nlohmann::json& value, const std::vector<std::string>& fallback)
		{
			if (!value.is_array()) return fallback;
			std::vector<std::string> out;
			for (const auto& item : value) if (item.is_string()) out.push_back(item.get<std::string>());
			return out.empty() ? fallback : out;
		}

		// Returns whether the value had them all
		bool read_floats(const nlohmann::json& value, float* out, size_t count)
		{
			if (!value.is_array()) return false;
			for (size_t i = 0; i < count && i < value.size(); ++i) out[i] = value[i].get<float>();
			return value.size() >= count;
		}
	}

	CharacterViewComponent::Look CharacterViewComponent::Look::from_json(const nlohmann::json& json, const Look& defaults)
	{
		Look look = defaults;
		if (!json.is_object()) return look;
		const nlohmann::json clips = json.value("clips", nlohmann::json::object());
		look.clips.idle = string_list(clips.value("idle", nlohmann::json()), look.clips.idle);
		look.clips.move = string_list(clips.value("move", nlohmann::json()), look.clips.move);
		look.clips.attack = string_list(clips.value("attack", nlohmann::json()), look.clips.attack);
		look.clips.death = string_list(clips.value("death", nlohmann::json()), look.clips.death);
		look.clips.fast = string_list(clips.value("fast", nlohmann::json()), look.clips.fast);
		look.clips.hit = string_list(clips.value("hit", nlohmann::json()), look.clips.hit);
		look.clips.knockDown = string_list(clips.value("knockDown", nlohmann::json()), look.clips.knockDown);
		look.clips.getUp = string_list(clips.value("getUp", nlohmann::json()), look.clips.getUp);
		look.clips.melee = string_list(clips.value("melee", nlohmann::json()), look.clips.melee);
		look.clips.fire = string_list(clips.value("fire", nlohmann::json()), look.clips.fire);
		look.clips.reload = string_list(clips.value("reload", nlohmann::json()), look.clips.reload);
		look.clips.walkFire = string_list(clips.value("walkFire", nlohmann::json()), look.clips.walkFire);
		look.clips.meleeIdle = string_list(clips.value("meleeIdle", nlohmann::json()), look.clips.meleeIdle);
		look.clips.gunIdle = string_list(clips.value("gunIdle", nlohmann::json()), look.clips.gunIdle);
		look.clips.meleeMove = string_list(clips.value("meleeMove", nlohmann::json()), look.clips.meleeMove);
		look.clips.meleeMoving = string_list(clips.value("meleeMoving", nlohmann::json()), look.clips.meleeMoving);
		if (clips.contains("actions") && clips["actions"].is_object())
		{
			for (auto it = clips["actions"].begin(); it != clips["actions"].end(); ++it)
			{
				look.clips.actions[it.key()] = string_list(it.value(), {});
			}
		}
		look.overlayRoot = json.value("overlayRoot", look.overlayRoot);
		look.height = json.value("height", look.height);
		look.pool = json.value("pool", 0);
		look.model = json.value("model", look.model);
		look.variants = string_list(json.value("variants", nlohmann::json()), look.variants);
		look.walkRef = json.value("walkRef", look.walkRef);
		look.runRef = json.value("runRef", look.runRef);
		look.fastAbove = json.value("fastAbove", look.fastAbove);
		read_floats(json.value("tint", nlohmann::json()), look.tint, 3);
		if (json.contains("weapons") && json["weapons"].is_object())
		{
			for (auto it = json["weapons"].begin(); it != json["weapons"].end(); ++it)
			{
				CharacterViewComponent::Held held;
				held.mesh = it.value().value("mesh", std::string());
				held.bone = it.value().value("bone", std::string("hand_r"));
				held.scale = it.value().value("scale", 1.0f);
				read_floats(it.value().value("offset", nlohmann::json()), held.offset, 3);
				read_floats(it.value().value("euler", nlohmann::json()), held.euler, 3);
				held.hasGrip = read_floats(it.value().value("grip", nlohmann::json()), held.grip, 3);
				held.twoHanded = it.value().value("twoHanded", false);
				look.weapons.push_back({ std::atoi(it.key().c_str()), held });
			}
		}
		if (json.contains("held") && json["held"].is_object())
		{
			const nlohmann::json& held = json["held"];
			look.hasHeld = true;
			look.held.mesh = held.value("mesh", std::string());
			look.held.bone = held.value("bone", std::string("hand_r"));
			look.held.scale = held.value("scale", 1.0f);
			read_floats(held.value("offset", nlohmann::json()), look.held.offset, 3);
			read_floats(held.value("euler", nlohmann::json()), look.held.euler, 3);
			look.held.hasGrip = read_floats(held.value("grip", nlohmann::json()), look.held.grip, 3);
			look.held.twoHanded = held.value("twoHanded", false);
		}
		const nlohmann::json reach = json.value("reach", nlohmann::json());
		if (reach.is_array() && reach.size() == 3)
		{
			for (size_t i = 0; i < 3; ++i) look.reach[i] = reach[i].get<std::string>();
		}
		return look;
	}

	std::array<float, 16> CharacterViewComponent::Held::placement() const
	{
		return local_matrix(offset, euler, scale);
	}

	bool CharacterViewComponent::Held::grip_in_bone(float out[3]) const
	{
		if (!hasGrip) return false;
		const Mat4 local = placement();
		out[0] = local[0] * grip[0] + local[4] * grip[1] + local[8] * grip[2] + local[12];
		out[1] = local[1] * grip[0] + local[5] * grip[1] + local[9] * grip[2] + local[13];
		out[2] = local[2] * grip[0] + local[6] * grip[1] + local[10] * grip[2] + local[14];
		return true;
	}

	CharacterViewComponent::CharacterViewComponent(std::unique_ptr<Pakal::MeshComponent3D_Bgfx> mesh, const Look& look) :
		m_mesh(std::move(mesh)),
		m_look(look)
	{
		if (m_mesh)
		{
			m_mesh->set_in_place(true); // movement comes from gameplay
			m_mesh->set_color(1.0f, 1.0f, 1.0f);
		}
	}

	CharacterViewComponent::~CharacterViewComponent() = default;


	std::unique_ptr<Pakal::MeshComponent3D_Bgfx> CharacterViewComponent::release_mesh()
	{
		return std::move(m_mesh);
	}

	std::unique_ptr<Pakal::MeshComponent3D_Bgfx> CharacterViewComponent::release_held()
	{
		return std::move(m_held);
	}

	void CharacterViewComponent::set_held(std::unique_ptr<Pakal::MeshComponent3D_Bgfx> held)
	{
		m_held = std::move(held);
		if (m_held && m_mesh && m_look.hasHeld)
		{
			// The renderer places it on the hand bone as it draws, so it never lags behind the body
			m_held->attach_to(m_mesh.get(), m_look.held.bone, m_look.held.placement().data());
			m_held->set_color(1.0f, 1.0f, 1.0f);
		}
	}

	void CharacterViewComponent::add_weapon(int consumableId, std::unique_ptr<Pakal::MeshComponent3D_Bgfx> mesh, const Held& held)
	{
		if (!mesh || !m_mesh)
		{
			return;
		}
		mesh->attach_to(m_mesh.get(), held.bone, held.placement().data());
		mesh->set_color(1.0f, 1.0f, 1.0f);
		mesh->set_visible(false);
		m_weaponMeshes[consumableId] = std::move(mesh);
		m_weaponHeld[consumableId] = held;
	}

	void CharacterViewComponent::sync(float dt, float alpha)
	{
		if (!m_mesh)
		{
			return;
		}
		Actor& self = actor();
		const b2Vec2 drawn = self.drawn_position(alpha);
		// off the floor (a jump): drawn that high
		const HeightComponent* body = self.get<HeightComponent>();
		const float raised = body ? body->drawn_height(alpha) : 0.0f;
		const b2Vec2 step = drawn - m_lastPosition;
		m_lastPosition = drawn;
		// Speed from the distance walked, smoothed: physics nudges make single frames noisy
		if (dt > 0.0f)
		{
			const float blend = std::min(1.0f, dt * 10.0f);
			m_speed += (step.Length() / dt - m_speed) * blend;
		}
		m_moving = m_speed > kMinFacingSpeed;
		m_hitTimer = std::max(0.0f, m_hitTimer - dt);
		m_hitCooldown = std::max(0.0f, m_hitCooldown - dt);

		bool dead = false, attacking = false, attackStarted = false, hurt = false, player = false, swingStarted = false;
		bool firingGun = false; // the player's gun is in a fire cycle
		bool burst = false;     // nothing is left of it
		int hurtClips = -1;     // an enemy's count of hurt clips
		bool knocked = false;   // knocked back...
		float knockLeft = 0.0f; // ... for this long still
		float lift = 0.0f, tumble = 0.0f; // a body flung into the air
		const CharacterAction* action = nullptr; // the step of a special attack
		int meleeId = 0;
		float attackSeconds = 0.0f;
		float sink = 0.0f;
		float r = 1.0f, g = 1.0f, b = 1.0f; // the flash of a hurt body
		bool faceStep = true; // face the way the body goes
		if (const CharacterViewSource* source = self.get<CharacterViewSource>())
		{
			const CharacterViewState state = source->view_state();
			dead = state.dead;
			attacking = state.attacking;
			burst = state.burst;
			hurtClips = state.hurtClips;
			knocked = state.knocked;
			knockLeft = state.knockLeft;
			lift = state.lift;
			tumble = state.tumble;
			if (state.special)
			{
				// its steps have clips of their own (the look's actions); it faces the way its brain turns it
				action = state.action;
				if (attacking)
				{
					m_yawTarget = actor_yaw(state.facing.x, -state.facing.y);
					m_yawKnown = true;
					faceStep = false;
				}
				attacking = false;
			}
			attackStarted = attacking && !m_wasAttacking;
			// the kit's clip is stretched over the original's whole attack clip (it stands that long)
			attackSeconds = state.attackSeconds;
			hurt = state.hurt;
			if (attacking && !action)
			{
				m_yawTarget = actor_yaw(state.attackDirection.x, -state.attackDirection.y);
				m_yawKnown = true;
				faceStep = false;
			}
			sink = state.sink;
			// The hurt flash is white, red while enraged (berserk zombies: Level::ToggleBerserkMode)
			if (hurt)
			{
				if (state.enraged) { r = 1.6f; g = 0.35f; b = 0.35f; }
				else { r = g = b = 1.6f; }
			}
			if (dead) faceStep = false;
		}
		else
		{
			const HealthComponent* health = self.get<HealthComponent>();
			const PlayerControlComponent* control = self.get<PlayerControlComponent>();
			player = control != nullptr;   // or a survivor walking with it (an ally): its clips follow its speed
			const WeaponPose* weapon = self.get<WeaponPose>();
			dead = health && health->is_dead();
			hurt = health && health->hurt_recently();
			attacking = weapon && (weapon->firing() || weapon->swinging());
			firingGun = weapon && weapon->firing() && !weapon->swinging();
			swingStarted = weapon && weapon->swinging() && !m_wasSwinging;
			m_wasSwinging = weapon && weapon->swinging();
			if (const ConsumableComponent* consumable = self.get<ConsumableComponent>())
			{
				if ((consumable->melee() || consumable->firearm()) && consumable->active()) meleeId = consumable->active()->id; // the model of the weapon lent
			}
			if (control) m_moving = control->moving();
			if (hurt) { r = 1.6f; g = 0.4f; b = 0.4f; } // the player flashes red (IsFlashRedHurt)
			else if (const ConsumableComponent* boost = self.get<ConsumableComponent>())
			{
				// A boost (speed, damage, defence) makes the body glow while it lasts: cyan, orange, blue
				const ConsumableRules* active = boost->active();
				if (active && !active->melee && !active->firearm)
				{
					m_boostClock += dt;
					const float pulse = 0.5f + 0.5f * std::sin(m_boostClock * 7.0f);
					if (active->speedFactor > 1.0f) { g += 0.35f * pulse; b += 0.45f * pulse; }
					if (active->damageFactor > 1.0f) { r += 0.6f * pulse; g += 0.15f * pulse; }
					if (active->defenceFactor > 1.0f) { b += 0.6f * pulse; r -= 0.1f * pulse; }
				}
			}
			faceStep = !player; // the player's facing is its aim (set by the control)
		}
		m_wasAttacking = attacking;

		const bool knockStarted = knocked && !m_wasKnocked;
		m_wasKnocked = knocked;
		// A new hit starts the hit clip, but not on every bullet (a zombie's brain says when)
		bool newHit = hurt && !m_wasHurt;
		m_wasHurt = hurt;
		if (hurtClips >= 0)
		{
			newHit = hurtClips != m_hurtClipsSeen;
			m_hurtClipsSeen = hurtClips;
		}
		if (newHit && !dead && m_hitCooldown <= 0.0f && !m_look.clips.hit.empty())
		{
			for (const auto& name : m_look.clips.hit)
			{
				const int index = m_mesh->find_animation(name);
				if (index >= 0)
				{
					m_mesh->play_animation(index, false);
					m_hitTimer = static_cast<float>(m_mesh->get_animation_length_seconds());
					m_hitCooldown = kHitCooldown + m_hitTimer;
					break;
				}
			}
		}

		// Turn toward the facing at the game's rotate speed
		if (faceStep && step.Length() > 1e-5f && m_speed > kMinFacingSpeed)
		{
			m_yawTarget = actor_yaw(step.x, -step.y);
			if (!m_yawKnown) { self.yaw = m_yawTarget; m_yawKnown = true; }
		}
		if (m_yawKnown && !player)
		{
			const float diff = angle_difference(m_yawTarget, self.yaw);
			const float most = kTurnSpeed * dt;
			self.yaw += (std::fabs(diff) <= most || dt <= 0.0f) ? diff : (diff > 0.0f ? most : -most);
		}

		// The body kicks back from each shot, against the way it aims
		b2Vec2 shown = drawn;
		if (player)
		{
			const WeaponPose* gun = self.get<WeaponPose>();
			const PlayerControlComponent* aim = self.get<PlayerControlComponent>();
			if (gun && aim)
			{
				shown -= (0.1f * gun->recoil()) * aim->facing();
			}
		}
		m_mesh->set_position(shown.x, raised + lift - sink * m_look.height * 0.5f, -shown.y);
		m_mesh->set_rotation(tumble, self.yaw, 0.0f);
		float lo[3], hi[3];
		m_mesh->get_bounds(lo, hi);
		const float height = hi[1] - lo[1];
		m_mesh->set_scale(burst ? 0.0f : (height > 0.001f ? m_look.height / height : 1.0f));
		m_mesh->set_color(m_look.tint[0] * r, m_look.tint[1] * g, m_look.tint[2] * b);

		// The clip: the speed picks walk or run, and the playback follows it so the feet do not slide
		const bool fast = m_speed > m_look.fastAbove && !m_look.clips.fast.empty();
		float clipSpeed = 1.0f;
		auto actionClips = action && !action->name.empty() ? m_look.clips.actions.find(action->name) : m_look.clips.actions.end();
		if (!m_forced.empty())
		{
			play_clip(*m_mesh, { m_forced }, true);
		}
		else if (!dead && actionClips != m_look.clips.actions.end())
		{
			// A step of a special attack: its clip from the start, over the step's time
			if (action->serial != m_actionSerial)
			{
				m_actionSerial = action->serial;
				m_actionSpeed = 1.0f;
				for (const auto& name : actionClips->second)
				{
					const int index = m_mesh->find_animation(name);
					if (index >= 0)
					{
						m_mesh->play_animation(index, action->loop);
						const double length = m_mesh->get_animation_length_seconds();
						if (length > 0.0 && action->seconds > 0.0f) m_actionSpeed = static_cast<float>(length / action->seconds);
						break;
					}
				}
			}
			clipSpeed = m_actionSpeed;
		}
		else if (dead)
		{
			m_knockStep = 0;
			play_clip(*m_mesh, m_look.clips.death, false);
		}
		else if ((knockStarted && knockLeft >= kKnockDownAbove && !m_look.clips.knockDown.empty()) || m_knockStep > 0)
		{
			// Knocked back: down, then up again, the getting up fitted into what is left of the stun
			auto play_once = [this](const std::vector<std::string>& names)
			{
				for (const auto& name : names)
				{
					const int index = m_mesh->find_animation(name);
					if (index >= 0)
					{
						m_mesh->play_animation(index, false);
						return static_cast<float>(m_mesh->get_animation_length_seconds());
					}
				}
				return 0.0f;
			};
			if (m_knockStep == 0)
			{
				m_knockStep = 1;
				m_knockClipLeft = play_once(m_look.clips.knockDown);
				m_actionSpeed = 1.0f;
			}
			m_knockClipLeft -= dt * m_actionSpeed;
			if (m_knockClipLeft <= 0.0f)
			{
				const float length = m_knockStep == 1 ? play_once(m_look.clips.getUp) : 0.0f;
				m_knockStep = length > 0.0f ? 2 : 0;
				m_knockClipLeft = length;
				m_actionSpeed = length > 0.0f ? std::max(1.0f, length / std::max(0.2f, knockLeft)) : 1.0f;
			}
			clipSpeed = m_actionSpeed;
		}
		else if (m_hitTimer > 0.0f)
		{
			// the hit clip plays out (it was started above)
		}
		else if (player && meleeId != 0 && !m_look.clips.melee.empty() && (swingStarted || m_wasSwingClip))
		{
			// One swing of the melee weapon (on the run if it has a running swing), then back to moving or standing
			if (swingStarted)
			{
				const auto& swings = m_moving && !m_look.clips.meleeMoving.empty() ? m_look.clips.meleeMoving : m_look.clips.melee;
				for (const auto& name : swings)
				{
					const int index = m_mesh->find_animation(name);
					if (index >= 0)
					{
						m_mesh->play_animation(index, false);
						m_swingClipSeconds = static_cast<float>(m_mesh->get_animation_length_seconds());
						break;
					}
				}
			}
			m_wasSwingClip = true;
			m_swingClipSeconds -= dt;
			if (m_swingClipSeconds <= 0.0f) m_wasSwingClip = false;
		}
		else if (attacking && !player)
		{
			// One swing per attack, timed so that it lands when the hit does
			if (attackStarted)
			{
				for (const auto& name : m_look.clips.attack)
				{
					const int index = m_mesh->find_animation(name);
					if (index >= 0)
					{
						m_mesh->play_animation(index, false);
						break;
					}
				}
				const double length = m_mesh->get_animation_length_seconds();
				m_attackClipSpeed = (length > 0.0 && attackSeconds > 0.0f) ? static_cast<float>(length / attackSeconds) : 1.0f;
			}
			clipSpeed = m_attackClipSpeed;
		}
		else if (m_moving)
		{
			// The player walks while firing (the legs of the walk clip) and runs otherwise; with a melee weapon it has
			// its own run
			const auto& legs = player && meleeId != 0 && !m_look.clips.meleeMove.empty() ? m_look.clips.meleeMove
				: (player && firingGun && !m_look.clips.walkFire.empty() ? m_look.clips.walkFire : (fast ? m_look.clips.fast : m_look.clips.move));
			play_clip(*m_mesh, legs, true);
			const float reference = &legs == &m_look.clips.walkFire ? m_look.walkRef : (fast ? m_look.runRef : m_look.walkRef);
			clipSpeed = std::clamp(m_speed / reference, 0.6f, 2.0f);
			// The player walking away from where it aims (backpedalling): the legs play backwards
			if (player)
			{
				const PlayerControlComponent* control = self.get<PlayerControlComponent>();
				const float length = step.Length();
				if (control && length > 1e-5f && b2Dot(step, control->facing()) < -kBackpedalCos * length)
				{
					clipSpeed = -clipSpeed;
				}
			}
		}
		else
		{
			// standing: a melee weapon's rest, a two-handed gun's (held in both hands), or the plain one
			const int restId = meleeId != 0 ? meleeId : m_baseWeapon;
			auto lent = m_weaponHeld.find(restId);
			const Held* rest = restId != 0 && lent != m_weaponHeld.end() && m_weaponMeshes.count(restId) ? &lent->second
				: (m_look.hasHeld ? &m_look.held : nullptr);
			const bool twoHanded = player && rest && rest->twoHanded && !m_look.clips.gunIdle.empty();
			play_clip(*m_mesh, player && meleeId != 0 && !m_look.clips.meleeIdle.empty() ? m_look.clips.meleeIdle
				: (twoHanded ? m_look.clips.gunIdle : m_look.clips.idle), true);
		}
		m_mesh->set_animation_speed(m_frozen ? 0.0f : clipSpeed);

		// The player's upper body: the fire clip once per fire cycle while firing, the reload clip over the reload time
		if (player)
		{
			const WeaponPose* weapon = self.get<WeaponPose>();
			auto find = [this](const std::vector<std::string>& names)
			{
				for (const auto& name : names)
				{
					const int index = m_mesh->find_animation(name);
					if (index >= 0) return index;
				}
				return -1;
			};
			const int reloadClip = find(m_look.clips.reload);
			const int fireClip = find(m_look.clips.fire);
			if (dead || meleeId != 0 || !weapon)
			{
				m_mesh->stop_overlay();
			}
			else if (weapon->reloading() && reloadClip >= 0)
			{
				if (m_mesh->get_overlay() != reloadClip || !m_mesh->overlay_playing())
				{
					const float seconds = std::max(0.1f, weapon->reload_seconds());
					m_mesh->play_overlay(reloadClip, m_look.overlayRoot, false, static_cast<float>(m_mesh->get_clip_length_seconds(reloadClip) / seconds));
				}
			}
			else if (firingGun && fireClip >= 0)
			{
				const float cycle = std::max(0.05f, weapon->cycle_seconds());
				m_mesh->play_overlay(fireClip, m_look.overlayRoot, true, static_cast<float>(m_mesh->get_clip_length_seconds(fireClip) / cycle));
			}
			else
			{
				m_mesh->stop_overlay();
			}
		}

		// The hand holds the gun (the equipped one), or the weapon a consumable lends
		const int shownId = meleeId != 0 ? meleeId : m_baseWeapon;
		const bool weaponShown = shownId != 0 && m_weaponMeshes.count(shownId) != 0;
		if (m_held)
		{
			m_held->set_visible(!weaponShown && !dead);
		}
		for (auto& entry : m_weaponMeshes)
		{
			entry.second->set_visible(entry.first == shownId && !dead);
		}

		// The other hand on the gun (two-bone IK of the look's 'reach' arm toward the weapon's grip): always on a long gun,
		// on a pistol while it fires; not while reloading, swinging or down. Blended in and out.
		if (player && !m_look.reach[0].empty())
		{
			const Held* held = nullptr;
			if (weaponShown) { auto found = m_weaponHeld.find(shownId); if (found != m_weaponHeld.end()) held = &found->second; }
			else if (m_look.hasHeld) held = &m_look.held;
			const WeaponPose* weapon = self.get<WeaponPose>();
			const bool busy = dead || (weapon && (weapon->reloading() || weapon->swinging()));
			static const bool reachOff = std::getenv("ZOMBIES_REACH") && std::string(std::getenv("ZOMBIES_REACH")) == "off";
			const bool wanted = held && held->hasGrip && !busy && !reachOff && (held->twoHanded || firingGun);
			const float step = std::min(1.0f, dt * 8.0f);
			m_reachWeight += ((wanted ? 1.0f : 0.0f) - m_reachWeight) * step;
			if (held && held->hasGrip && m_reachWeight > 0.01f)
			{
				// the grip in the bone's frame: the weapon's placement in the hand times the grip in the weapon
				Held fitted = *held;
				// Debug: ZOMBIES_REACH=x,y,z puts the grip there (in the weapon's model) to fit it; =off turns the reach off
				static const char* forced = std::getenv("ZOMBIES_REACH");
				static float forcedGrip[3] = { 0.0f, 0.0f, 0.0f };
				static const bool hasForced = forced && std::sscanf(forced, "%f,%f,%f", &forcedGrip[0], &forcedGrip[1], &forcedGrip[2]) == 3;
				if (hasForced) std::copy(forcedGrip, forcedGrip + 3, fitted.grip);
				float at[3];
				fitted.grip_in_bone(at);
				m_mesh->set_reach(m_look.reach[0], m_look.reach[1], m_look.reach[2], held->bone, at, m_reachWeight);
			}
			else
			{
				m_mesh->set_reach(std::string(), std::string(), std::string(), std::string(), nullptr, 0.0f);
			}
		}
	}
}
