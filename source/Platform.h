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
//#define PAKAL_STATIC_LIB	//
#define PAKAL_NONCLIENT_BUILD

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
#	endif
#elif defined( __APPLE_CC__ ) // Apple OS X
// Device                                       Simulator
#   if __IPHONE_OS_VERSION_MIN_REQUIRED >= 20201 || __IPHONE_OS_VERSION_MIN_REQUIRED >= 20000
//#   if __IPHONE_OS_VERSION_MIN_REQUIRED >= 30000 || __IPHONE_OS_VERSION_MIN_REQUIRED >= 30000
#       define PAKAL_IPHONE_PLATFORM
#   else
#       define PAKAL_APPLE_PLATFORM
#   endif
#elif defined(OS_ANDROID) || defined(ANDROID) || defined(__ANDROID__)
#	define PAKAL_ANDROID_PLATFORM
#elif defined(__linux__)
#	define PAKAL_LINUX_PLATFORM
//#endif
#endif

////////////////////////////////////////////////////////////
// Define helpers to create portable import / export macros for each module
////////////////////////////////////////////////////////////
#if defined(PAKAL_STATIC_LIB)

    // Static build doesn't need import/export macros
    #define PAKAL_API_EXPORT
    #define PAKAL_API_IMPORT
#else

	#if defined(PAKAL_WIN32_PLATFORM)

        // Windows compilers need specific (and different) keywords for export and import
        #define PAKAL_API_EXPORT __declspec(dllexport)
        #define PAKAL_API_IMPORT __declspec(dllimport)

        // For Visual C++ compilers, we also need to turn off this annoying C4251 warning
        #ifdef _MSC_VER

            #pragma warning(disable: 4251)

        #endif

    #else // Linux, FreeBSD, Mac OS X

        #if __GNUC__ >= 4

            // GCC 4 has special keywords for showing/hidding symbols,
            // the same keyword is used for both importing and exporting
            #define PAKAL_API_EXPORT __attribute__ ((__visibility__ ("default")))
            #define PAKAL_API_IMPORT __attribute__ ((__visibility__ ("default")))

        #else

            // GCC < 4 has no mechanism to explicitely hide symbols, everything's exported
            #define PAKAL_API_EXPORT
            #define PAKAL_API_IMPORT

        #endif

    #endif

#endif

#if defined( PAKAL_NONCLIENT_BUILD )
#	define _PAKALExport PAKAL_API_EXPORT
#else
#   define _PAKALExport PAKAL_API_IMPORT
#endif

////////////////////////////////////////////////////////////
// Define several useful macros
////////////////////////////////////////////////////////////
#define SAFE_DEL(x) {delete (x);x=nullptr;}

#ifdef _DEBUG
	void assert_with_message(bool condition, const char *format, ...);

	#define __ASSERT_QUOTE__(x) #x
	#define __ASSERT_TO_STRING__(x) __ASSERT_QUOTE__(x)
	#define __ASSERT_INFO__ "assert: file [" __FILE__ ":" __ASSERT_TO_STRING__(__LINE__) "]"

	#define ASSERT(x) assert_with_message(x, __ASSERT_INFO__)	
	#define ASSERT_MSG(x, format, ...) assert_with_message(x, __ASSERT_INFO__ "\n" format, ## __VA_ARGS__)

	
#else
	#define  ASSERT(...)
	#define ASSERT_MSG(...)
#endif

