///////////////////////////////////////////////////////////////////////////////////////////////////////
// Copyright (C) 2012 The PakalEngine Open Source Project
// File: Engine.h
// Original Author: Salvador Noel Romo Garcia.
///////////////////////////////////////////////////////////////////////////////////////////////////////


#pragma once

#include "config.h"
#include "Platform.h"
#include "core/Application.h"
#include "core/Engine.h"

#ifdef TEO_WIN32_PLATFORM
	#ifdef _DEBUG
		#pragma comment(lib, "zw.lib")	// engine name
		//#pragma comment(lib, "tinythread.lib")
		//#pragma comment(lib, "RocketCore.lib")
		//#pragma comment(lib, "RocketControls.lib")		
	#else
		#pragma comment(lib, "pakal.lib")
		#pragma comment(lib, "tinythread.lib")
		//#pragma comment(lib, "RocketCore.lib")
		//#pragma comment(lib, "RocketControls.lib")				
	#endif

#endif

#ifdef TEO_WIN32_PLATFORM
	#include <windows.h>
#elif defined(TEO_ANDROID_PLATFORM)
	#include <jni.h>
	#include <errno.h>

	#include <EGL/egl.h>
	#include <GLES/gl.h>

	#include <android/sensor.h>
	#include <android/log.h>

#else
	
#endif