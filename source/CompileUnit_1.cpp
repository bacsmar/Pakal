///////////////////////////////////////////////////////////////////////////////////////////////////////
// Copyright (C) 2012 The PakalEngine Open Source Project
// File: Engine.h
// Original Author: Salvador Noel Romo Garcia.
///////////////////////////////////////////////////////////////////////////////////////////////////////

#include "config.h"

#ifdef PAKAL_WIN32_PLATFORM
	#ifdef _DEBUG
		//#pragma comment(lib, "Pakal_II.lib")	// engine name
		#pragma comment(lib, "box2d.lib")
		#pragma comment(lib, "Irrlicht.lib") 
		#pragma comment(lib, "lua.lib")
		#pragma comment(lib, "oolua.lib")		
		//#pragma comment(lib, "PocoFoundationmdd.lib")
		//#pragma comment(lib, "tinythread.lib")
#if ROCKET_STATIC_LIB
		//#pragma comment(lib, "RocketCore.lib")
		//#pragma comment(lib, "RocketControls.lib")
#endif		
	#else
		//#pragma comment(lib, "Pakal_II.lib")
		//#pragma comment(lib, "tinythread.lib")
		//#pragma comment(lib, "RocketCore.lib")
		//#pragma comment(lib, "RocketControls.lib")				
	#endif

#endif

#ifdef PAKAL_WIN32_PLATFORM
	#include <windows.h>
#elif defined(PAKAL_ANDROID_PLATFORM)
	#include <jni.h>
	#include <errno.h>

	#include <EGL/egl.h>
	#include <GLES/gl.h>

	#include <android/sensor.h>
	#include <android/log.h>

#else
	
#endif
