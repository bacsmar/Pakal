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
#include <string>

#if PAKAL_USE_LOG == 1
	#ifndef _DEBUG

	#define LOG_DEBUG(format, ...)		Pakal::LogMgr::instance().log(Pakal::LogMgr::LOG_DEBUG,"[DEBUG]\t"    format, ## __VA_ARGS__);
	#define LOG_INFO(format, ...)		Pakal::LogMgr::instance().log(Pakal::LogMgr::LOG_INFO,"[INFO]\t"    format, ## __VA_ARGS__);
	#define LOG_WARNING(format, ...)	Pakal::LogMgr::instance().log(Pakal::LogMgr::LOG_WARNING,"[WARNING]\t" format, ## __VA_ARGS__);
	#define LOG_ERROR(format, ...)		Pakal::LogMgr::instance().log(Pakal::LogMgr::LOG_ERROR,"[ERROR]\t"   format, ## __VA_ARGS__);
	#define LOG_FATAL(format, ...)		Pakal::LogMgr::instance().log(Pakal::LogMgr::LOG_FATAL,"[FATAL]\t"   format, ## __VA_ARGS__);

	#else /* Not NDEBUG */
#if PAKAL_LOG_SHOW_FILE_INFO == 1
	#define _LOG_QUOTE_AUX_(x) #x
	#define _LOG_QUOTE_(x) _LOG_QUOTE_AUX_(x)
	#define __LOG_INFO_FORMAT__ "%-80s"
	#define __LOG_INFO__ "[" __FILE__ ":" _LOG_QUOTE_(__LINE__) "]: "
#else
	#define __LOG_INFO_FORMAT__ "%s"
	#define __LOG_INFO__ ""
#endif

	#define LOG_DEBUG(format, ...)		Pakal::LogMgr::instance().log(Pakal::LogMgr::LOG_DEBUG, "[DEBUG]\t"   __LOG_INFO_FORMAT__ format, __LOG_INFO__, ## __VA_ARGS__);
	#define LOG_INFO(format, ...)		Pakal::LogMgr::instance().log(Pakal::LogMgr::LOG_INFO, "[INFO]\t"    __LOG_INFO_FORMAT__ format, __LOG_INFO__, ## __VA_ARGS__);
	#define LOG_WARNING(format, ...)	Pakal::LogMgr::instance().log(Pakal::LogMgr::LOG_WARNING, "[WARNING]\t" __LOG_INFO_FORMAT__ format, __LOG_INFO__, ## __VA_ARGS__);
	#define LOG_ERROR(format, ...)		Pakal::LogMgr::instance().log(Pakal::LogMgr::LOG_ERROR, "[ERROR]\t"   __LOG_INFO_FORMAT__ format, __LOG_INFO__, ## __VA_ARGS__);
	#define LOG_FATAL(format, ...)		Pakal::LogMgr::instance().log(Pakal::LogMgr::LOG_FATAL, "[FATAL]\t"   __LOG_INFO_FORMAT__ format, __LOG_INFO__, ## __VA_ARGS__);

	#endif // NDEBUG
#else
	#define LOG_DEBUG(format, ...)
	#define LOG_INFO(format, ...)		
	#define LOG_WARNING(format, ...)	
	#define LOG_ERROR(format, ...)		
	#define LOG_FATAL(format, ...)		
#endif

// forward declaration

#ifdef PAKAL_IPHONE_PLATFORM
typedef	struct __sFILE FILE;
#elif defined(PAKAL_ANDROID_PLATFORM)
//typedef struct _sFILE FILE;
#elif defined(PAKAL_LINUX_PLATFORM)
//*nothing*//
#else
typedef struct _iobuf FILE;
#endif

namespace Pakal{	

	class _PAKALExport LogMgr
	{
		template <typename T>
		friend class SingletonHolder;

		FILE    *m_log;
		std::string mFileName;
	protected:		
		LogMgr();
		~LogMgr(void);

	public:				
		void setFile(const std::string &_filename);

		void operator<<(const std::string& val);		

		static LogMgr& instance();

		enum LogLevel{ LOG_DEBUG, LOG_INFO, LOG_WARNING, LOG_ERROR, LOG_FATAL};
		
		void inline log(const std::string&msg) { log(LOG_INFO, msg.c_str()); }
		void log( int level, const char *format, ... );
		void log(int level, const char *format, va_list& args);
	};
}
