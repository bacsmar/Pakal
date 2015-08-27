///////////////////////////////////////////////////////////////////////////////////////////////////////
// Copyright (C) 2011 The PakalEngine Open Source Project
// File: IEntity.h
// Original Author: Salvador Noel Romo Garcia.
// last modification: 13 jun 2015
// Purpose: Interface for Entities.
///////////////////////////////////////////////////////////////////////////////////////////////////////

#pragma once
#include "Config.h"
#include "TaskFwd.h"

namespace Pakal
{
	class IComponent;

	class _PAKALExport IEntity
	{
	public:	
		virtual ~IEntity(){}
		virtual BasicTaskPtr initialize() = 0;
		virtual BasicTaskPtr destroy() = 0;
	};

	
}