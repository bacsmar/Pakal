///////////////////////////////////////////////////////////////////////////////////////////////////////
// Copyright (C) 2026 The PakalEngine Open Source Project
// File: PlayerController.cpp
// Purpose: Player input and movement controller component - Implementation
///////////////////////////////////////////////////////////////////////////////////////////////////////

#include "PlayerController.h"
#include "Health.h"
#include "Weapon.h"
#include "Entity.h"
#include "LogMgr.h"
#include "components/SpriteComponent2D.h"
#include "components/SpritePhysicsComponent.h"
#include "InputManager_Polling.h"

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
		m_inputMoveLeft(false),
		m_inputMoveRight(false),
		m_inputJump(false),
		m_inputFire(false),
		m_currentAnim(IDLE)
	{
	}
	
	void PlayerController::initialize()
	{
		Entity* parent = get_parent_entity();
		if (!parent)
			return;
		
		// Get component references
		m_physics = parent->get_component<SpritePhysicsComponent>();
		m_sprite = parent->get_component<SpriteComponent2D>();
		m_weapon = parent->get_component<Weapon>();
		m_health = parent->get_component<Health>();
		
		if (!m_physics || !m_sprite)
		{
			LOG_WARNING("PlayerController: Missing physics or sprite component");
		}
	}
	
	void PlayerController::update(float deltaTime)
	{
		if (!m_health || !m_health->is_alive())
			return;
		
		handle_input();
		handle_movement(deltaTime);
		check_ground_collision();
		update_animation();
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
		
		// Apply horizontal velocity based on input
		if (m_inputMoveLeft)
		{
			m_physics->set_lineal_velocity(tmath::vector2df(-m_moveSpeed, currentVel.y));
		}
		else if (m_inputMoveRight)
		{
			m_physics->set_lineal_velocity(tmath::vector2df(m_moveSpeed, currentVel.y));
		}
		else
		{
			// No movement input: slow down horizontal velocity
			m_physics->set_lineal_velocity(tmath::vector2df(0.0f, currentVel.y));
		}
		
		// Handle jumping
		if (m_inputJump && m_isGrounded && !m_isJumping)
		{
			m_physics->apply_impulse(tmath::vector2df(0.0f, m_jumpForce));
			m_isJumping = true;
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
	}
	
	void PlayerController::check_ground_collision()
	{
		// TODO: Use Box2D raycasts to check if player is on ground
		// For now, simple implementation: check if player is not too far above last known ground
		if (!m_physics)
		{
			m_isGrounded = false;
			return;
		}
		
		// Simple heuristic: if Y velocity is near zero or negative (falling), and was jumping, allow landing
		auto vel = m_physics->get_lineal_velocity();
		
		// If moving downward or standing still, consider grounded
		if (vel.y <= 0.5f)  // Small threshold to avoid jitter
		{
			m_isGrounded = true;
			m_isJumping = false;
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
		
		// Update sprite flip based on facing direction
		// Note: SpriteComponent2D may not have set_flip method; check actual API
		// m_sprite->set_flip_horizontal(!m_facingRight);
		
		// Update animation state based on movement
		AnimState prevAnim = m_currentAnim;
		
		if (!m_isGrounded)
		{
			m_currentAnim = JUMP;
		}
		else if (m_inputMoveLeft || m_inputMoveRight)
		{
			m_currentAnim = RUN;
		}
		else if (m_inputFire)
		{
			m_currentAnim = SHOOT;
		}
		else
		{
			m_currentAnim = IDLE;
		}
		
		// When animation changes, could trigger animation switch in sprite component
		// m_sprite->play_animation(animationName);
	}
}
