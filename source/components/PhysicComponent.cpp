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
		return m_PhysicsSystem->get_scheduler()->execute_in_thread(std::bind(&PhysicComponent::onInit,this),m_PhysicsSystem->get_thread_id());
	}
	BasicTaskPtr PhysicComponent::destroy()
	{
		return m_PhysicsSystem->get_scheduler()->execute_in_thread(std::bind(&PhysicComponent::onDestroy,this),m_PhysicsSystem->get_thread_id());
	}
}