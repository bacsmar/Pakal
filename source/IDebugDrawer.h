///////////////////////////////////////////////////////////////////////////////////////////////////////
// Copyright (C) 2011 The Pakal Engine Open Source Project
// File: IDebugDrawer.h
// Original Author: Salvador Noel Romo Garcia.
// Creation Date: 01-11-2012
// last modification: 13 jun 2015
// Purpose: AsyncComponent Interface.
//
///////////////////////////////////////////////////////////////////////////////////////////////////////
#pragma once
#include "Config.h"

typedef class NullRenderInfo {} BaseRenderInfo;

namespace Pakal
{
	struct _PAKALExport RendererInfo : public BaseRenderInfo
	{
		virtual ~RendererInfo() {}
	};

	class _PAKALExport IDebugDrawerClient
	{
	public:
		virtual ~IDebugDrawerClient(void){}	
		IDebugDrawerClient(){}
				
		virtual void do_debug_draw()	= 0;
		virtual void set_drawer(const RendererInfo* renderInfo)	= 0;
	};			
}
