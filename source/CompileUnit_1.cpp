///////////////////////////////////////////////////////////////////////////////////////////////////////
// Copyright (C) 2012 The PakalEngine Open Source Project
// File: CompileUnit_1.cpp
// Original Author: Salvador Noel Romo Garcia.
///////////////////////////////////////////////////////////////////////////////////////////////////////

#include "Config.h"

// pugixml, compiled once for the engine (its .cpp is left out of the globs)
#include <persist/pugixml/pugixml.cpp>

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
