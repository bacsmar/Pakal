///////////////////////////////////////////////////////////////////////////////////////////////////////
// Copyright (C) 2011 The Pakal Engine Open Source Project
// File: IDebugDrawer.h
// Original Author: Salvador Noel Romo Garcia.
// Creation Date: 01-11-2012
// last modification: 13 jun 2015
// Purpose: Component Interface.
//
///////////////////////////////////////////////////////////////////////////////////////////////////////
#pragma once
#include "config.h"

#ifdef PAKAL_USE_IRRLICHT
	#include "irrlicht/IrrRendererInfo.h"
	typedef Pakal::IrrRendererInfo BaseRenderInfo;
#else
	typedef class NullRenderInfo {} BaseRenderInfo;
#endif

namespace Pakal
{
	struct _PAKALExport RendererInfo : public BaseRenderInfo
	{
		virtual ~RendererInfo() {}
	};

	class _PAKALExport IDebugDrawerClient
	{
	public:
		//virtual ~IDebugDrawerClient(void){}	
		IDebugDrawerClient(){}
				
		virtual void doDebugDraw()	= 0;
		virtual void setDrawer(const RendererInfo *renderInfo)	= 0;
	};			
}
