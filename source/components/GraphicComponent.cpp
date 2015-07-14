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


namespace Pakal
{
	BasicTaskPtr GraphicComponent::init()
	{
		return m_GraphicSystem->initComponentAsync(this);
	}
	BasicTaskPtr GraphicComponent::destroy()
	{
		return m_GraphicSystem->terminateComponentAsync(this);
	}
	///////////////////////////////////////////////////////////////////////////////////////////////////////
	GraphicComponent::~GraphicComponent()
	{
	}
	///////////////////////////////////////////////////////////////////////////////////////////////////////
	void GraphicComponent::notify()
	{
		ASSERT( isInitialized() );		
		m_GraphicSystem->addToUpdateList(this);
	}	
}