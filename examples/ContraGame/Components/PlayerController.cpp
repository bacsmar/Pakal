///////////////////////////////////////////////////////////////////////////////////////////////////////
// Copyright (C) 2026 The PakalEngine Open Source Project
// File: PlayerController.cpp
// Purpose: Player input and movement controller component - Implementation
///////////////////////////////////////////////////////////////////////////////////////////////////////

#include "PlayerController.h"
#include "Health.h"
#include "Weapon.h"
#include "Entity.h"
#include "GenericEntity.h"
#include "LogMgr.h"
#include "components/SpriteComponent2D.h"
#include "components/SpritePhysicsComponent.h"
#include "InputManager_Polling.h"
#include <cmath>

namespace
{
	constexpr float kGroundedVelocityThreshold = 0.18f;
	constexpr float kGroundedHeightTolerance = 0.15f;
	constexpr float kJumpUpVelocityThreshold = 0.2f;
	constexpr float kJumpFallVelocityThreshold = -0.2f;

	constexpr int kIdleStart = 0;
	constexpr int kIdleEnd = 3;
	constexpr int kRunStart = 4;
	constexpr int kRunEnd = 11;
	constexpr int kJumpUpFrame = 12;
	constexpr int kJumpFallFrame = 13;
	constexpr int kShootStart = 14;
	constexpr int kShootEnd = 17;
}

namespace Pakal
{
	PlayerController::PlayerController() :
		m_physics(nullptr),
		m_sprite(nullptr),
		m_weapon(nullptr),
		m_health(nullptr),
		m_moveSpeed(5.0f),
		m_jumpForce(10.0f),
		m_isGrounded(false),
		m_isJumping(false),
		m_facingRight(true),
		m_lastGroundedY(0.0f),
		m_timeSinceJump(999.0f),
		m_shootAnimTimer(0.0f),
		m_shootAnimDuration(0.12f),
		m_coyoteTimer(999.0f),
		m_coyoteTimeWindow(0.10f),
		m_inputMoveLeft(false),
		m_inputMoveRight(false),
		m_inputJump(false),
		m_inputFire(false),
		m_prevInputJump(false),
		m_prevInputFire(false),
		m_currentAnim(IDLE)
	{
	}
	
	void PlayerController::initialize()
	{
		Entity* parent = get_parent_entity();
		if (!parent)
			return;

		auto* genericParent = dynamic_cast<GenericEntity*>(parent);
		if (!genericParent)
			return;
		
		// Get component references
		m_physics = genericParent->get_component<SpritePhysicsComponent>();
		m_sprite = genericParent->get_component<SpriteComponent2D>();
		m_weapon = genericParent->get_component<Weapon>();
		m_health = genericParent->get_component<Health>();
		
		if (!m_physics || !m_sprite)
		{
			LOG_WARNING("PlayerController: Missing physics or sprite component");
		}

		if (m_physics)
		{
			m_lastGroundedY = m_physics->get_position().y;
		}
	}
	
	void PlayerController::update(float deltaTime)
	{
		if (!m_health || !m_health->is_alive())
			return;

		m_timeSinceJump += deltaTime;
		m_coyoteTimer += deltaTime;
		if (m_shootAnimTimer > 0.0f)
		{
			m_shootAnimTimer -= deltaTime;
		}
		
		handle_input();
		handle_movement(deltaTime);
		check_ground_collision();
		update_animation();

		m_prevInputJump = m_inputJump;
		m_prevInputFire = m_inputFire;
	}
	
	void PlayerController::handle_input()
	{
		// Poll input from InputManager_Polling
		InputManager_Polling& input = InputManager_Polling::instance();
		
		m_inputMoveLeft = input.is_move_left_pressed();
		m_inputMoveRight = input.is_move_right_pressed();
		m_inputJump = input.is_jump_pressed();
		m_inputFire = input.is_fire_pressed();
	}
	
	void PlayerController::handle_movement(float deltaTime)
	{
		(void)deltaTime;
		if (!m_physics)
			return;
		
		// Update facing direction based on input
		if (m_inputMoveRight)
		{
			m_facingRight = true;
		}
		else if (m_inputMoveLeft)
		{
			m_facingRight = false;
		}
		
		// Get current velocity to preserve Y velocity when moving horizontally
		auto currentVel = m_physics->get_lineal_velocity();
		
		// Apply horizontal velocity based on input (only touch X — let Box2D own Y/gravity)
		if (m_inputMoveLeft)
		{
			m_physics->set_horizontal_velocity(-m_moveSpeed);
		}
		else if (m_inputMoveRight)
		{
			m_physics->set_horizontal_velocity(m_moveSpeed);
		}
		else
		{
			// No movement input: stop horizontal velocity
			m_physics->set_horizontal_velocity(0.0f);
		}
		
		// Handle jumping
		const bool jumpPressedThisFrame = m_inputJump && !m_prevInputJump;
		const bool canCoyoteJump = (!m_isGrounded && m_coyoteTimer <= m_coyoteTimeWindow);
		if (jumpPressedThisFrame && (m_isGrounded || canCoyoteJump) && !m_isJumping)
		{
			// Zero vel.y via set_lineal_velocity before impulse so jump height is predictable
			m_physics->set_lineal_velocity(tmath::vector2df(currentVel.x, 0.0f));
			m_physics->apply_impulse(tmath::vector2df(0.0f, m_jumpForce));
			m_isJumping = true;
			m_isGrounded = false;
			m_timeSinceJump = 0.0f;
			m_coyoteTimer = m_coyoteTimeWindow + 1.0f;
		}
		
		// Handle firing
		if (m_inputFire)
		{
			handle_shooting();
		}
	}
	
	void PlayerController::handle_shooting()
	{
		if (!m_weapon || !m_weapon->can_fire())
			return;
		
		// Fire in the direction the player is facing
		tmath::vectorn<float, 2> direction(m_facingRight ? 1.0f : -1.0f, 0.0f);
		m_weapon->fire(direction);
		m_shootAnimTimer = m_shootAnimDuration;
	}
	
	void PlayerController::check_ground_collision()
	{
		if (!m_physics)
		{
			m_isGrounded = false;
			return;
		}
		
		const auto pos = m_physics->get_position();
		auto vel = m_physics->get_lineal_velocity();

		const bool closeToGroundHeight = pos.y <= (m_lastGroundedY + kGroundedHeightTolerance);
		const bool verticalMotionStable = std::fabs(vel.y) <= kGroundedVelocityThreshold;
		const bool canLandNow = m_timeSinceJump > 0.08f;

		if (closeToGroundHeight && verticalMotionStable && canLandNow)
		{
			m_isGrounded = true;
			m_isJumping = false;
			m_lastGroundedY = pos.y;
			m_coyoteTimer = 0.0f;
		}
		else
		{
			m_isGrounded = false;
		}
	}
	
	void PlayerController::update_animation()
	{
		if (!m_sprite)
			return;
		
		// Keep visual facing synced with movement direction.
		m_sprite->set_flip(!m_facingRight, false);
		
		AnimState prevAnim = m_currentAnim;
		tmath::vector2df vel(0.0f, 0.0f);
		if (m_physics)
		{
			vel = m_physics->get_lineal_velocity();
		}
		
		if (m_shootAnimTimer > 0.0f)
		{
			m_currentAnim = SHOOT;
		}
		else if (!m_isGrounded)
		{
			m_currentAnim = (vel.y >= kJumpUpVelocityThreshold) ? JUMP_UP : JUMP_FALL;
		}
		else if (m_inputMoveLeft || m_inputMoveRight)
		{
			m_currentAnim = RUN;
		}
		else
		{
			m_currentAnim = IDLE;
		}
		
		if (prevAnim != m_currentAnim)
		{
			switch (m_currentAnim)
			{
			case IDLE:
				m_sprite->play_animation(kIdleStart, kIdleEnd, true);
				break;
			case RUN:
				m_sprite->play_animation(kRunStart, kRunEnd, true);
				break;
			case JUMP_UP:
				m_sprite->play_animation(kJumpUpFrame, kJumpUpFrame, false);
				break;
			case JUMP_FALL:
				m_sprite->play_animation(kJumpFallFrame, kJumpFallFrame, false);
				break;
			case SHOOT:
				m_sprite->play_animation(kShootStart, kShootEnd, false);
				break;
			}
		}
	}
}
