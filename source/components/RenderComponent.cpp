///////////////////////////////////////////////////////////////////////////////////////////////////////
// Copyright (C) 2011 The PakalEngine Open Source Project
// File: IEntity.h
// Original Author: Salvador Noel Romo Garcia.
// last modification: 13 jun 2015
// Purpose: Interface for Entities.
///////////////////////////////////////////////////////////////////////////////////////////////////////

#pragma once
#include "RenderComponent.h"
#include "GraphicsSystem.h"


namespace Pakal
{
	BasicTaskPtr RenderComponent::init()
	{
		return m_RenderSystem->initComponentAsync(this);
	}
	BasicTaskPtr RenderComponent::destroy()
	{
		return m_RenderSystem->terminateComponentAsync(this);
	}
	///////////////////////////////////////////////////////////////////////////////////////////////////////
	RenderComponent::~RenderComponent()
	{
	}
	///////////////////////////////////////////////////////////////////////////////////////////////////////
	void RenderComponent::notify()
	{
		ASSERT( isInitialized() );		
		m_RenderSystem->addToUpdateList(this);
	}
	/*
	void RenderComponent::notify(std::function<void()> lambda)
	{
		ASSERT( isInitialized() );		
		m_RenderSystem->addToUpdateList(this, lamda);
	}
	///////////////////////////////////////////////////////////////////////////////////////////////////////

	BasicTaskPtr LoadMesh( std::string meshName)
	{
		auto lambda = [=]()
		{
			//load mesh
		}

		notify(lambda);
	}

	void update()
	{
		 for ( auto l : m_lamdas)
		 {
			l(); 
		}
	}
	*/
}