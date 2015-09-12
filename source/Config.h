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

#define PAKAL_COMPILE_WITH_EDITOR 0

//! compile with LUA Scripts
#define PAKAL_USE_SCRIPTS 0
//! compile with ogre3D
#define PAKAL_USE_OGRE 0
//! compile with Irrlicht
#define PAKAL_USE_IRRLICHT 1

//! compile with http://www.sfml-dev.org
#define PAKAL_USE_SFML 0 
#define PAKAL_USE_SFML_AUDIO 1 
#define PAKAL_USE_SFML_INPUT 1 

//! compile with bullet physics
#define PAKAL_USE_BULLET 0

//! compile with BOX2D physics
#define PAKAL_USE_BOX2D 1

//! compile with Rocket GUI
#define PAKAL_USE_ROCKET 1

//! compile with DEBUGGER
#define PAKAL_USE_DEBUGGER 0

//! compile with PROFILER
#define PAKAL_USE_PROFILER 0

//! compile with LOG
#define PAKAL_USE_LOG 1
//! include file & line information
#define PAKAL_LOG_SHOW_FILE_INFO 0
#define PAKAL_USE_LOG_FILE 0
