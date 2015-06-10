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

#if PAKAL_USE_LOG == 1
	#ifndef _DEBUG

	#define LOG_DEBUG(format, ...)		Pakal::LogMgr::log(Pakal::LogMgr::LOG_DEBUG,"[DEBUG]\t"    format, ## __VA_ARGS__);
	#define LOG_INFO(format, ...)		Pakal::LogMgr::log(Pakal::LogMgr::LOG_INFORMATION,"[INFO]\t"    format, ## __VA_ARGS__);
	#define LOG_WARNING(format, ...)	Pakal::LogMgr::log(Pakal::LogMgr::LOG_WARNING,"[WARNING]\t" format, ## __VA_ARGS__);
	#define LOG_ERROR(format, ...)		Pakal::LogMgr::log(Pakal::LogMgr::LOG_ERROR,"[ERROR]\t"   format, ## __VA_ARGS__);
	#define LOG_FATAL(format, ...)		Pakal::LogMgr::log(Pakal::LogMgr::LOG_FATAL,"[FATAL]\t"   format, ## __VA_ARGS__);

	#else /* Not NDEBUG */

	#define _LOG_QUOTE_AUX_(x) #x
	#define _LOG_QUOTE_(x) _LOG_QUOTE_AUX_(x)
	#define __LOG_INFO_FORMAT__ "%-80s"
	#define __LOG_INFO__ "[" __FILE__ ":" _LOG_QUOTE_(__LINE__) "]: "

	#define LOG_DEBUG(format, ...)		Pakal::LogMgr::log(Pakal::LogMgr::LOG_DEBUG, "[DEBUG]\t"   __LOG_INFO_FORMAT__ format, __LOG_INFO__, ## __VA_ARGS__);
	#define LOG_INFO(format, ...)		Pakal::LogMgr::log(Pakal::LogMgr::LOG_INFORMATION, "[INFO]\t"    __LOG_INFO_FORMAT__ format, __LOG_INFO__, ## __VA_ARGS__);
	#define LOG_WARNING(format, ...)	Pakal::LogMgr::log(Pakal::LogMgr::LOG_WARNING, "[WARNING]\t" __LOG_INFO_FORMAT__ format, __LOG_INFO__, ## __VA_ARGS__);
	#define LOG_ERROR(format, ...)		Pakal::LogMgr::log(Pakal::LogMgr::LOG_ERROR, "[ERROR]\t"   __LOG_INFO_FORMAT__ format, __LOG_INFO__, ## __VA_ARGS__);
	#define LOG_FATAL(format, ...)		Pakal::LogMgr::log(Pakal::LogMgr::LOG_FATAL, "[FATAL]\t"   __LOG_INFO_FORMAT__ format, __LOG_INFO__, ## __VA_ARGS__);

	#endif // NDEBUG
#else
	#define LOG_DEBUG(format, ...)
	#define LOG_INFO(format, ...)		
	#define LOG_WARNING(format, ...)	
	#define LOG_ERROR(format, ...)		
	#define LOG_FATAL(format, ...)		
#endif

namespace Pakal{	

	namespace LogMgr 
	{
		enum LogLevel{ LOG_NONE, LOG_FATAL, LOG_CRITICAL, LOG_ERROR, LOG_WARNING, LOG_NOTICE, LOG_INFORMATION, LOG_DEBUG, LOG_TRACE};

		void log( int level, const char *format, ... );

		void setLogLevel(int level);

		bool init();
	};
}

#if PAKAL_USE_SCRIPTS == 1

class  TLog
{
	//Pakal::LogMgr *pManager;
public:	
	TLog() { //pManager = Pakal::LogMgr::instance(); 
			 }
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
