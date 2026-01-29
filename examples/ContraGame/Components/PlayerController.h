///////////////////////////////////////////////////////////////////////////////////////////////////////
// Copyright (C) 2026 The PakalEngine Open Source Project
// File: PlayerController.h
// Creation Date: January 2026
//
// Purpose: Player input and movement controller
//
///////////////////////////////////////////////////////////////////////////////////////////////////////
#pragma once

#include "Component.h"
#include "PakalMath.h"

namespace Pakal
{
	class IInputManager;
	class SpriteComponent;
	class Health;
	class Weapon;
	class PhysicComponent;

	class _PAKALExport PlayerController : public Component
	{
		DECLARE_RTTI_WITH_BASE(PlayerController, Component);

	public:
		PlayerController();
		~PlayerController() override = default;

		void initialize();
		void update(float deltaTime);

		// Configuration
		void set_move_speed(float speed) { m_moveSpeed = speed; }
		void set_jump_force(float force) { m_jumpForce = force; }
		void set_max_fall_speed(float speed) { m_maxFallSpeed = speed; }

		// State queries
		bool is_grounded() const { return m_isGrounded; }
		bool is_jumping() const { return m_isJumping; }
		float get_velocity_x() const { return m_velocityX; }
		bool is_facing_right() const { return m_facingRight; }

		// Input enable/disable
		void set_input_enabled(bool enabled) { m_inputEnabled = enabled; }
		bool is_input_enabled() const { return m_inputEnabled; }

	private:
		void handle_input(float deltaTime);
		void handle_movement(float deltaTime);
		void handle_shooting();
		void update_animation();
		void check_ground();

		// Components (cached references)
		PhysicComponent* m_physics;
		SpriteComponent* m_sprite;
		Weapon* m_weapon;
		Health* m_health;
		IInputManager* m_inputManager;

		// Movement parameters
		float m_moveSpeed;
		float m_jumpForce;
		float m_maxFallSpeed;

		// State
		bool m_isGrounded;
		bool m_isJumping;
		bool m_facingRight;
		bool m_inputEnabled;
		float m_velocityX;
		float m_velocityY;
		float m_groundCheckDistance;

		// Input state
		bool m_moveLeftPressed;
		bool m_moveRightPressed;
		bool m_jumpPressed;
		bool m_shootPressed;
		bool m_jumpReleased;

		// Animation
		enum class AnimState
		{
			IDLE,
			RUN,
			JUMP,
			FALL,
			SHOOT,
			SHOOT_RUN
		};
		AnimState m_currentAnim;
		AnimState m_previousAnim;
		float m_animTime;

		// Timing
		float m_coyoteTime;          // Time after leaving platform where you can still jump
		float m_coyoteTimeCounter;
		float m_jumpBufferTime;      // Time to buffer jump input
		float m_jumpBufferCounter;
	};
}
