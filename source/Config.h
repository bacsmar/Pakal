///////////////////////////////////////////////////////////////////////////////////////////////////////
// Copyright (C) 2012 The PakalEngine Open Source Project
// File: Config.h
// Original Author: Salvador Noel Romo Garcia.
// Creation Date: october 31 2012
//
// Purpose: Compilation config
//
///////////////////////////////////////////////////////////////////////////////////////////////////////

#include "Platform.h"

#pragma once

#define TEO_COMPILE_WITH_EDITOR 0

//! compile with LUA Scripts
#define TEO_USE_SCRIPTS 0
//! compile with ogre3D
#define TEO_USE_OGRE 0
//! compile with Irrlicht
#define TEO_USE_IRRLICHT 1

//! compile with http://www.sfml-dev.org
#define TEO_USE_SFML 0 

//! compile with bullet physics
#define TEO_USE_BULLET 0

//! compile with BOX2D physics
#define TEO_USE_BOX2D 1

//! compile with OIS
#define TEO_USE_OIS 0 

#if COMPILE_FOR_EDITOR || defined(ANDROID) || TEO_USE_SFML
#undef TEO_USE_OIS
#define TEO_USE_OIS 0
#endif

//! compile with Rocket GUI
#define TEO_USE_ROCKET 1

//! compile with CAUDIO
#define TEO_USE_CAUDIO 0

//! compile with DEBUGGER
#define TEO_USE_DEBUGGER 1

//! compile with PROFILER
#define TEO_USE_PROFILER 1

//! compile with LOG
#define TEO_USE_LOG 1
#define TEO_USE_LOG_FILE 0

//! compile with multithread
#define TEO_USE_THREADS 0