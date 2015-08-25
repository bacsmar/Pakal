///////////////////////////////////////////////////////////////////////////////////////////////////////
// Copyright (C) 2011 The PakalEngine Open Source Project
// File: IEntity.h
// Original Author: Salvador Noel Romo Garcia.
// last modification: 13 jun 2015
// Purpose: Interface for Entities.
///////////////////////////////////////////////////////////////////////////////////////////////////////

#pragma once
#include "GraphicComponent.h"
#include "GraphicsSystem.h"
#include "EventScheduler.h"


namespace Pakal
{
	///////////////////////////////////////////////////////////////////////////////////////////////////////
	GraphicComponent::~GraphicComponent()
	{
	}

	BasicTaskPtr GraphicComponent::init()
	{
		return m_graphic_system->get_scheduler()->execute_in_thread(std::bind(&GraphicComponent::on_init,this),m_graphic_system->get_thread_id());
	}

	BasicTaskPtr GraphicComponent::destroy()
	{
		return m_graphic_system->get_scheduler()->execute_in_thread(std::bind(&GraphicComponent::on_destroy,this),m_graphic_system->get_thread_id());
	}

}