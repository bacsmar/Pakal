///////////////////////////////////////////////////////////////////////////////////////////////////////
// Copyright (C) 2011 The PakalEngine Open Source Project
// File: PhysicComponent.cpp
// Original Author: Salvador Noel Romo Garcia.
// last modification: 13 jun 2015
///////////////////////////////////////////////////////////////////////////////////////////////////////

#pragma once
#include "PhysicComponent.h"
#include "PhysicsSystem.h"

namespace Pakal
{
	BasicTaskPtr PhysicComponent::init()
	{
		return m_PhysicsSystem->initComponentAsync(this);
	}
}