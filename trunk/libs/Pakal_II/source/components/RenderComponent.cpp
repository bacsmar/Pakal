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
	void RenderComponent::initAsync()
	{
		m_RenderSystem->initComponentAsync(this);
	}
}