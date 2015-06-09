///////////////////////////////////////////////////////////////////////////////////////////////////////
// Copyright (C) 2011 The ZombieWar Engine Open Source Project
// File: LogMgr.h
// Original Author: Salvador Noel Romo Garcia.
//
// Purpose: Tool used for log output
//
///////////////////////////////////////////////////////////////////////////////////////////////////////

#pragma once
#include "config.h"

#if TEO_USE_LOG == 1
	#ifndef _DEBUG

	#define LOG_DEBUG(format, ...)		ZombieWar::LogMgr::log(ZombieWar::LogMgr::LOG_DEBUG,"[DEBUG]\t"    format, ## __VA_ARGS__);
	#define LOG_INFO(format, ...)		ZombieWar::LogMgr::log(ZombieWar::LogMgr::LOG_INFO,"[INFO]\t"    format, ## __VA_ARGS__);
	#define LOG_WARNING(format, ...)	ZombieWar::LogMgr::log(ZombieWar::LogMgr::LOG_WARNING,"[WARNING]\t" format, ## __VA_ARGS__);
	#define LOG_ERROR(format, ...)		ZombieWar::LogMgr::log(ZombieWar::LogMgr::LOG_ERROR,"[ERROR]\t"   format, ## __VA_ARGS__);
	#define LOG_FATAL(format, ...)		ZombieWar::LogMgr::log(ZombieWar::LogMgr::LOG_FATAL,"[FATAL]\t"   format, ## __VA_ARGS__);

	#else /* Not NDEBUG */

	#define _LOG_QUOTE_AUX_(x) #x
	#define _LOG_QUOTE_(x) _LOG_QUOTE_AUX_(x)
	#define __LOG_INFO_FORMAT__ "%-80s"
	#define __LOG_INFO__ "[" __FILE__ ":" _LOG_QUOTE_(__LINE__) "]: "

	#define LOG_DEBUG(format, ...)		ZombieWar::LogMgr::log(ZombieWar::LogMgr::LOG_DEBUG, "[DEBUG]\t"   __LOG_INFO_FORMAT__ format, __LOG_INFO__, ## __VA_ARGS__);
	#define LOG_INFO(format, ...)		ZombieWar::LogMgr::log(ZombieWar::LogMgr::LOG_INFO, "[INFO]\t"    __LOG_INFO_FORMAT__ format, __LOG_INFO__, ## __VA_ARGS__);
	#define LOG_WARNING(format, ...)	ZombieWar::LogMgr::log(ZombieWar::LogMgr::LOG_WARNING, "[WARNING]\t" __LOG_INFO_FORMAT__ format, __LOG_INFO__, ## __VA_ARGS__);
	#define LOG_ERROR(format, ...)		ZombieWar::LogMgr::log(ZombieWar::LogMgr::LOG_ERROR, "[ERROR]\t"   __LOG_INFO_FORMAT__ format, __LOG_INFO__, ## __VA_ARGS__);
	#define LOG_FATAL(format, ...)		ZombieWar::LogMgr::log(ZombieWar::LogMgr::LOG_FATAL, "[FATAL]\t"   __LOG_INFO_FORMAT__ format, __LOG_INFO__, ## __VA_ARGS__);

	#endif // NDEBUG
#else
	#define LOG_DEBUG(format, ...)
	#define LOG_INFO(format, ...)		
	#define LOG_WARNING(format, ...)	
	#define LOG_ERROR(format, ...)		
	#define LOG_FATAL(format, ...)		
#endif

namespace ZombieWar{	

	namespace LogMgr 
	{

		enum LogLevel{ LOG_DEBUG, LOG_INFO, LOG_WARNING, LOG_ERROR, LOG_FATAL};

		void log( int level, const char *format, ... );
	};
}

#if TEO_USE_SCRIPTS == 1

class  TLog
{
	ZombieWar::LogMgr *pManager;
public:	
	TLog() { pManager = ZombieWar::LogMgr::instance(); }
	// funciones miembro de las clases huesped
	inline void log(const std::string& msg) { LOG_INFO("[Script] %s", msg.c_str()); }
};

#include "oolua.h"

OOLUA_CLASS_NO_BASES(TLog)//class has no bases
OOLUA_NO_TYPEDEFS
OOLUA_ONLY_DEFAULT_CONSTRUCTOR	
OOLUA_MEM_FUNC(void,log, const std::string&)
//OOLUA_MEM_FUNC_CONST(Pakal::UIManager, instance, void)
OOLUA_CLASS_END

// no Olvide registrar en el cpp ->
#endif
