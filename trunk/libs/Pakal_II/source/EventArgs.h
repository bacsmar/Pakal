///////////////////////////////////////////////////////////////////////////////////////////////////////
// Copyright (C) 2015 The PakalEngine Open Source Project
// File: EventArgs.h
// Original Author: Salvador Noel Romo Garcia.
// Creation Date: 03-sep-2015
//
// Purpose: This file contains common  arguments used in Events
//
///////////////////////////////////////////////////////////////////////////////////////////////////////

#pragma once
#include "config.h"
#include <functional>
#include "RTTI.h"


namespace Pakal
{
	struct MouseArgs
		{
			unsigned x, y;
			unsigned button_id;
		};
	struct WindowArgs
		{
			unsigned windowId = 0;
			unsigned size_x = 0;
			unsigned size_y = 0;
		};
		
}