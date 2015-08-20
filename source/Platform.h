///////////////////////////////////////////////////////////////////////////////////////////////////////
// Copyright (C) 2012 The PakalEngine Open Source Project
// File: Platform.h
// Original Author: Salvador Noel Romo Garcia.
// Creation Date: october 30 2012
// Specification Document:
//
// Purpose: Platform specific defines
//
///////////////////////////////////////////////////////////////////////////////////////////////////////


#pragma once
//-------------- Version -------------------------------------------//
#define PAKAL_VERSION_MAJOR 2
#define PAKAL_VERSION_MINOR 0
#define PAKAL_VERSION_PATCH 0
#define PAKAL_VERSION_NAME "2.0.0"

#define PAKAL_VERSION ((PAKAL_VERSION_MAJOR << 16) | (PAKAL_VERSION_MINOR << 8) | PAKAL_VERSION_PATCH)

//------------------------------------------------------------------//

#ifdef PAKAL_DYNAMIC_LIB
#  undef PAKAL_DYNAMIC_LIB
#endif

#define PAKAL_DYNAMIC_LIB

#define PAKAL_NONCLIENT_BUILD

// Default is blank for most OS's
#define _PAKALExport

//-------------- Determine Compiler ---------------------------------
#if defined( _MSC_VER )
#	define PAKAL_MSVC_COMPILER
#elif defined( __GNUC__ )
#	if defined( __WIN32__ ) || defined( _WIN32 )
#		define PAKAL_MINGW_COMPILER
#	else
#		define PAKAL_GCC_COMPILER
#	endif
#elif defined( __BORLANDC__ )
#	define PAKAL_BORLAND_COMPILER
#else
#	error No Recognized Compiler!
#endif

// --------------- Determine Operating System Platform ---------------
#if defined( __WIN32__ ) || defined( _WIN32 ) // Windows 2000, XP, ETC
#	if defined ( _XBOX )
#		define PAKAL_XBOX_PLATFORM
#	else
#		define PAKAL_WIN32_PLATFORM
#		if defined( PAKAL_DYNAMIC_LIB )
#			undef _PAKALExport
//Ignorable Dll interface warning...
#           if !defined(PAKAL_MINGW_COMPILER)
#			    pragma warning (disable : 4251)
#           endif
#			if defined( PAKAL_NONCLIENT_BUILD )
#				define _PAKALExport __declspec( dllexport )
#			else
#               if defined(PAKAL_MINGW_COMPILER)
#                   define _PAKALExport
#               else
#				    define _PAKALExport __declspec( dllimport )
#               endif
#			endif
#		endif
#	endif
#elif defined( __APPLE_CC__ ) // Apple OS X
// Device                                       Simulator
#   if __IPHONE_OS_VERSION_MIN_REQUIRED >= 20201 || __IPHONE_OS_VERSION_MIN_REQUIRED >= 20000
//#   if __IPHONE_OS_VERSION_MIN_REQUIRED >= 30000 || __IPHONE_OS_VERSION_MIN_REQUIRED >= 30000
#       define PAKAL_IPHONE_PLATFORM
#   else
#       define PAKAL_APPLE_PLATFORM
#   endif
#   undef _PAKALExport
#   define _PAKALExport __attribute__((visibility("default")))
#elif defined(OS_ANDROID) || defined(ANDROID)
#	define PAKAL_ANDROID_PLATFORM
#elif defined(__linux__)
#	define PAKAL_LINUX_PLATFORM
//#endif
#endif

#define SAFE_DEL(x) {delete (x);x=nullptr;}

#ifdef _DEBUG
	#define ASSERT(x) assert (x)
	#include <cassert>
#else
	#define  ASSERT(x)
	#define  ASSERT_IF(x)
#endif

