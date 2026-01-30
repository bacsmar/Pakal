///////////////////////////////////////////////////////////////////////////////////////////////////////
// Copyright (C) 2026 The PakalEngine Open Source Project
// File: PlayerController.h
// Purpose: Player input and movement controller component
///////////////////////////////////////////////////////////////////////////////////////////////////////

#pragma once
#include "Component.h"
#include "math/vector2.h"

namespace Pakal
{
	class SpritebodyComponent_Box2D;
	class SpriteComponent_Bgfx;
	class Weapon;
	class Health;
	
	class _PAKALExport PlayerController : public Component
	{
		DECLARE_RTTI_WITH_BASE(PlayerController, Component);
		
	public:
		PlayerController();
		virtual ~PlayerController() = default;
		
		void initialize();
		void update(float deltaTime);
		
		// Configuration
		void set_move_speed(float speed) { m_moveSpeed = speed; }
		void set_jump_force(float force) { m_jumpForce = force; }
		
		// State queries
		bool is_grounded() const { return m_isGrounded; }
		bool is_jumping() const { return m_isJumping; }
		
	private:
		void handle_input();
		void handle_movement(float deltaTime);
		void handle_shooting();
		void update_animation();
		void check_ground_collision();
		
		// Components (obtained in initialize())
		SpritebodyComponent_Box2D* m_physics;
		SpriteComponent_Bgfx* m_sprite;
		Weapon* m_weapon;
		Health* m_health;
		
		// Parameters
		float m_moveSpeed;
		float m_jumpForce;
		
		// State
		bool m_isGrounded;
		bool m_isJumping;
		bool m_facingRight;
		
		// Animation
		enum AnimState { IDLE, RUN, JUMP, SHOOT };
		AnimState m_currentAnim;
	};
}
