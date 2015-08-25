///////////////////////////////////////////////////////////////////////////////////////////////////////
// Copyright (C) 2011 The PakalEngine Open Source Project
// File: PhysicComponent.cpp
// Original Author: Salvador Noel Romo Garcia.
// last modification: 13 jun 2015
///////////////////////////////////////////////////////////////////////////////////////////////////////

#pragma once
#include "PhysicComponent.h"
#include "PhysicsSystem.h"
#include "EventScheduler.h"

namespace Pakal
{
	BasicTaskPtr PhysicComponent::init()
	{
		return m_physics_system->get_scheduler()->execute_in_thread(std::bind(&PhysicComponent::on_init,this),m_physics_system->get_thread_id());
	}
	BasicTaskPtr PhysicComponent::destroy()
	{
		return m_physics_system->get_scheduler()->execute_in_thread(std::bind(&PhysicComponent::on_destroy,this),m_physics_system->get_thread_id());
	}
}