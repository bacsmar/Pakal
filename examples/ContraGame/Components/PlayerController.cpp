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

// Note: We'll include the actual component headers when linking
// For now, using forward declarations

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
		m_currentAnim(IDLE)
	{
	}
	
	void PlayerController::initialize()
	{
		Entity* parent = get_parent_entity();
		if (!parent)
			return;
		
		// Get component references
		// TODO: Uncomment when components are linked
		// m_physics = parent->get_component<SpritebodyComponent_Box2D>();
		// m_sprite = parent->get_component<SpriteComponent_Bgfx>();
		m_weapon = parent->get_component<Weapon>();
		m_health = parent->get_component<Health>();
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
		// TODO: Integrate with SFML input system
		// For now, this is a placeholder
		
		// Movement input would be checked here:
		// - Left/Right arrow or A/D keys
		// - Space for jump
		// - X/Z for shoot
	}
	
	void PlayerController::handle_movement(float deltaTime)
	{
		if (!m_physics)
			return;
		
		// TODO: Apply forces to physics body based on input
		// Example:
		// if (moving_right)
		//     m_physics->set_lineal_velocity(m_moveSpeed, current_y_velocity);
		// if (jumping && m_isGrounded)
		//     m_physics->apply_impulse(0, m_jumpForce);
	}
	
	void PlayerController::handle_shooting()
	{
		if (!m_weapon || !m_weapon->can_fire())
			return;
		
		// Fire in the direction the player is facing
		tmath::vector2df direction(m_facingRight ? 1.0f : -1.0f, 0.0f);
		m_weapon->fire(direction);
	}
	
	void PlayerController::check_ground_collision()
	{
		// TODO: Use Box2D raycasts to check if player is on ground
		// For now, this is a placeholder
		m_isGrounded = true;
	}
	
	void PlayerController::update_animation()
	{
		if (!m_sprite)
			return;
		
		// Update sprite flip based on facing direction
		// m_sprite->set_flip(!m_facingRight, false);
		
		// Update animation state based on movement
		// if (not grounded) -> JUMP animation
		// else if (moving) -> RUN animation  
		// else -> IDLE animation
	}
}
