///////////////////////////////////////////////////////////////////////////////////////////////////////
// Copyright (C) 2026 The PakalEngine Open Source Project
// File: PlayerController.h
// Purpose: Player input and movement controller component
///////////////////////////////////////////////////////////////////////////////////////////////////////

#pragma once
#include "Component.h"
#include "math/vector2.h"
#include "InputManager_Polling.h"

namespace Pakal
{
	class SpritePhysicsComponent;
		class SpriteComponent2D;
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
		SpritePhysicsComponent* m_physics;
		SpriteComponent2D* m_sprite;
		Weapon* m_weapon;
		Health* m_health;
		
		// Parameters
		float m_moveSpeed;
		float m_jumpForce;
		
		// State
		bool m_isGrounded;
		bool m_isJumping;
		bool m_facingRight;
		float m_lastGroundedY;
		float m_timeSinceJump;
		float m_shootAnimTimer;
		float m_shootAnimDuration;
		float m_coyoteTimer;
		float m_coyoteTimeWindow;
		
		// Input state (cached from InputManager_Polling)
		bool m_inputMoveLeft;
		bool m_inputMoveRight;
		bool m_inputJump;
		bool m_inputFire;
		bool m_prevInputJump;
		bool m_prevInputFire;
		
		// Animation
		enum AnimState { IDLE, RUN, JUMP_UP, JUMP_FALL, SHOOT };
		AnimState m_currentAnim;
	};
}
