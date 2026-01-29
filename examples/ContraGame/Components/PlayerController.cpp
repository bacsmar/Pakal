///////////////////////////////////////////////////////////////////////////////////////////////////////
// Copyright (C) 2026 The PakalEngine Open Source Project
// File: PlayerController.cpp
// Creation Date: January 2026
//
// Purpose: Player input and movement controller implementation
//
///////////////////////////////////////////////////////////////////////////////////////////////////////

#include "PlayerController.h"
#include "Health.h"
#include "Weapon.h"
#include "Entity.h"
#include "IInputManager.h"
#include "components/SpriteComponent.h"
#include "components/PhysicComponent.h"

using namespace Pakal;

IMPLEMENT_RTTI(PlayerController, Component);

PlayerController::PlayerController()
	: m_physics(nullptr)
	, m_sprite(nullptr)
	, m_weapon(nullptr)
	, m_health(nullptr)
	, m_inputManager(nullptr)
	, m_moveSpeed(200.0f)
	, m_jumpForce(400.0f)
	, m_maxFallSpeed(500.0f)
	, m_isGrounded(false)
	, m_isJumping(false)
	, m_facingRight(true)
	, m_inputEnabled(true)
	, m_velocityX(0.0f)
	, m_velocityY(0.0f)
	, m_groundCheckDistance(0.1f)
	, m_moveLeftPressed(false)
	, m_moveRightPressed(false)
	, m_jumpPressed(false)
	, m_shootPressed(false)
	, m_jumpReleased(true)
	, m_currentAnim(AnimState::IDLE)
	, m_previousAnim(AnimState::IDLE)
	, m_animTime(0.0f)
	, m_coyoteTime(0.1f)
	, m_coyoteTimeCounter(0.0f)
	, m_jumpBufferTime(0.1f)
	, m_jumpBufferCounter(0.0f)
{
}

void PlayerController::initialize()
{
	Entity* entity = get_parent_entity();
	if (!entity)
		return;

	// Get component references
	m_sprite = entity->get_component<SpriteComponent>();
	m_weapon = entity->get_component<Weapon>();
	m_health = entity->get_component<Health>();
	m_physics = entity->get_component<PhysicComponent>();

	// TODO: Get input manager from engine
	// m_inputManager = engine->input_manager();
}

void PlayerController::update(float deltaTime)
{
	if (!m_inputEnabled)
		return;

	// Check if player is alive
	if (m_health && !m_health->is_alive())
	{
		return;
	}

	// Update weapon cooldown
	if (m_weapon)
	{
		m_weapon->update(deltaTime);
	}

	// Handle input
	handle_input(deltaTime);

	// Update ground check
	check_ground();

	// Handle movement physics
	handle_movement(deltaTime);

	// Handle shooting
	handle_shooting();

	// Update animation
	update_animation();

	// Update timers
	m_animTime += deltaTime;
	if (m_coyoteTimeCounter > 0.0f)
		m_coyoteTimeCounter -= deltaTime;
	if (m_jumpBufferCounter > 0.0f)
		m_jumpBufferCounter -= deltaTime;
}

void PlayerController::handle_input(float deltaTime)
{
	if (!m_inputManager)
		return;

	// Read input states
	// TODO: Implement proper key binding system
	// For now, hardcoded keys:
	// WASD / Arrow keys for movement
	// Space for jump
	// X/Z for shoot

	// Movement input
	bool leftPressed = false;   // m_inputManager->is_key_down(Key::Left) || m_inputManager->is_key_down(Key::A);
	bool rightPressed = false;  // m_inputManager->is_key_down(Key::Right) || m_inputManager->is_key_down(Key::D);
	bool jumpPressed = false;   // m_inputManager->is_key_down(Key::Space);
	bool shootPressed = false;  // m_inputManager->is_key_down(Key::X) || m_inputManager->is_key_down(Key::Z);

	m_moveLeftPressed = leftPressed;
	m_moveRightPressed = rightPressed;
	m_jumpPressed = jumpPressed;
	m_shootPressed = shootPressed;

	// Jump buffer - allows jump input slightly before landing
	if (jumpPressed)
	{
		m_jumpBufferCounter = m_jumpBufferTime;
	}

	// Track jump release for jump cut
	if (!jumpPressed && m_jumpPressed)
	{
		m_jumpReleased = true;
	}
}

void PlayerController::handle_movement(float deltaTime)
{
	// Horizontal movement
	float targetVelocityX = 0.0f;

	if (m_moveLeftPressed)
	{
		targetVelocityX = -m_moveSpeed;
		m_facingRight = false;
	}
	else if (m_moveRightPressed)
	{
		targetVelocityX = m_moveSpeed;
		m_facingRight = true;
	}

	// Smooth acceleration (lerp)
	float acceleration = 0.2f;
	m_velocityX = m_velocityX + (targetVelocityX - m_velocityX) * acceleration;

	// Handle jumping
	bool canJump = (m_isGrounded || m_coyoteTimeCounter > 0.0f) && !m_isJumping;
	
	if (canJump && m_jumpBufferCounter > 0.0f)
	{
		m_velocityY = m_jumpForce;
		m_isJumping = true;
		m_jumpReleased = false;
		m_jumpBufferCounter = 0.0f;
		m_coyoteTimeCounter = 0.0f;
	}

	// Variable jump height (cut jump short when button released)
	if (m_isJumping && m_jumpReleased && m_velocityY > 0.0f)
	{
		m_velocityY *= 0.5f;
	}

	// Apply gravity (done by physics system normally)
	// m_velocityY -= gravity * deltaTime;

	// Cap fall speed
	if (m_velocityY < -m_maxFallSpeed)
	{
		m_velocityY = -m_maxFallSpeed;
	}

	// Apply velocity to physics body
	if (m_physics)
	{
		// TODO: Apply velocity to Box2D body
		// m_physics->set_linear_velocity({m_velocityX, m_velocityY});
	}
	else if (m_sprite)
	{
		// Fallback: directly update position if no physics
		auto pos = m_sprite->get_position();
		pos.x += m_velocityX * deltaTime;
		pos.y += m_velocityY * deltaTime;
		m_sprite->set_position(pos);
	}

	// Update sprite facing
	if (m_sprite)
	{
		m_sprite->set_flipped(!m_facingRight);
	}
}

void PlayerController::handle_shooting()
{
	if (!m_weapon || !m_shootPressed)
		return;

	if (m_weapon->can_fire())
	{
		// Shoot in facing direction
		tmath::vector2df direction(m_facingRight ? 1.0f : -1.0f, 0.0f);
		m_weapon->fire_at_direction(direction);
	}
}

void PlayerController::update_animation()
{
	if (!m_sprite)
		return;

	AnimState newAnim = m_currentAnim;

	// Determine animation state
	bool isMoving = (m_moveLeftPressed || m_moveRightPressed) && std::abs(m_velocityX) > 10.0f;
	bool isShooting = m_shootPressed;

	if (m_isGrounded)
	{
		if (isShooting && isMoving)
		{
			newAnim = AnimState::SHOOT_RUN;
		}
		else if (isShooting)
		{
			newAnim = AnimState::SHOOT;
		}
		else if (isMoving)
		{
			newAnim = AnimState::RUN;
		}
		else
		{
			newAnim = AnimState::IDLE;
		}
	}
	else
	{
		if (m_velocityY > 0.0f)
		{
			newAnim = AnimState::JUMP;
		}
		else
		{
			newAnim = AnimState::FALL;
		}
	}

	// Change animation if different
	if (newAnim != m_currentAnim)
	{
		m_previousAnim = m_currentAnim;
		m_currentAnim = newAnim;
		m_animTime = 0.0f;

		// Set sprite animation
		// TODO: Map AnimState to sprite animation names
		/*
		switch (m_currentAnim)
		{
		case AnimState::IDLE:
			m_sprite->set_animation("idle");
			break;
		case AnimState::RUN:
			m_sprite->set_animation("run");
			break;
		case AnimState::JUMP:
			m_sprite->set_animation("jump");
			break;
		case AnimState::FALL:
			m_sprite->set_animation("fall");
			break;
		case AnimState::SHOOT:
			m_sprite->set_animation("shoot");
			break;
		case AnimState::SHOOT_RUN:
			m_sprite->set_animation("shoot_run");
			break;
		}
		*/
	}
}

void PlayerController::check_ground()
{
	// Simplified ground check
	// TODO: Use Box2D ray-cast to detect ground below player
	
	// For now, check velocity
	bool wasGrounded = m_isGrounded;
	
	if (m_velocityY <= 0.0f)
	{
		// TODO: Actual ray-cast implementation
		// m_isGrounded = physics_raycast_down();
		
		// Temporary stub
		m_isGrounded = (m_velocityY == 0.0f);
	}
	else
	{
		m_isGrounded = false;
	}

	// Landing
	if (m_isGrounded && !wasGrounded)
	{
		m_isJumping = false;
		m_jumpReleased = true;
	}

	// Left ground (started falling)
	if (!m_isGrounded && wasGrounded)
	{
		m_coyoteTimeCounter = m_coyoteTime;
	}
}
