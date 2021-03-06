#include "LogMgr.h"
#include <iostream>
#include <stdarg.h>
#include <cstdio>

#ifdef ANDROID
	#include <jni.h>
	#include <android/log.h>
#endif
#include "TaskFwd.h"
#include "SingletonHolder.h"

using namespace Pakal;

#ifdef PAKAL_WIN32_PLATFORM
	#define COLOURED_LOG
#endif

#ifdef COLOURED_LOG
#include  <stdio.h>
#include  <windows.h>

void ChangeColour(WORD theColour)
{
	HANDLE hConsole = GetStdHandle(STD_OUTPUT_HANDLE);  // Get handle to standard output
	SetConsoleTextAttribute(hConsole,theColour);  // set the text attribute of the previous handle
}

#endif

namespace Pakal
{
	std::mutex logMutex;

	void LogMgr::operator<<(const std::string& msj )
	{
		mutex_guard guard(logMutex);
		log(LOG_INFO, msj.c_str());
	}

	LogMgr& LogMgr::instance()
	{
		static SingletonHolder<LogMgr> sh;
		return *sh.get();
	}	

	void LogMgr::log( int level, const char *format, ... )
	{
#if PAKAL_USE_LOG == 1
		va_list listArguments;
		va_start( listArguments, format );				

		log(level, format, listArguments);

		va_end( listArguments );
#endif
	}

	void LogMgr::log(int level, const char *format, va_list& args)
	{

#if PAKAL_USE_LOG == 1
		mutex_guard guard(logMutex);
		if (m_log == nullptr) return;

#ifdef COLOURED_LOG
		switch (level)
		{
		case LOG_DEBUG:
			ChangeColour(FOREGROUND_GREEN | FOREGROUND_INTENSITY);
			break;
		case LOG_INFO:
			ChangeColour(FOREGROUND_GREEN | FOREGROUND_BLUE | FOREGROUND_RED);
			break;
		case LOG_WARNING:
			ChangeColour(FOREGROUND_GREEN | FOREGROUND_RED | FOREGROUND_INTENSITY);
			break;
		case LOG_ERROR:
			ChangeColour(FOREGROUND_RED | FOREGROUND_INTENSITY);
			break;
		case LOG_FATAL:
			ChangeColour(FOREGROUND_GREEN | FOREGROUND_BLUE | FOREGROUND_INTENSITY);
			break;
		}
#endif

#ifndef PAKAL_ANDROID_PLATFORM
		vfprintf(m_log, format, args);
		fprintf(m_log, "\n");
		fflush(m_log);
#else
		__android_log_vprint(level + ANDROID_LOG_DEBUG, "PAKAL_ENGINE", format, args);
#endif

#if PAKAL_USE_LOG_FILE == 1						
		vfprintf(stdout, format, args);
		fprintf(stdout, "\n");
		fflush(stdout);
#endif		
#endif
	}

	LogMgr::LogMgr()
	{		
		m_log = stdout;
		mFileName.clear();
	}
	LogMgr::~LogMgr(void)
	{		
		log(LOG_INFO,"[INFO]\tLog deinitialized");
		if( m_log != stdout )
		{
			fclose( m_log );
		}
	}

	void LogMgr::setFile( const std::string &_filename )
	{		
		mutex_guard guard(logMutex);
		mFileName = _filename;

		if( mFileName.empty() == true)
			mFileName = "GameLog.log";
#if PAKAL_USE_LOG_FILE
#ifdef _MSC_VER
		fopen_s(&m_log,mFileName.c_str(), "w+");
#else
		m_log = fopen( mFileName.c_str(), "w" );
#endif		
#endif
		if( m_log == nullptr )
		{
			m_log = stdout;
			log(LOG_ERROR,"[ERROR]\tError at open the log file. Redirecting to stdout.");
		}

		log(LOG_INFO,"[INFO]\tLog redirected to %s", mFileName.c_str() );
	}
}

namespace rapidxml
{
	void parse_error_handler(const char *what, void *where)
	{
		LOG_ERROR("[RapidXML] error:%s",what)
	}
}

#ifdef _DEBUG
#include <cassert>
void assert_with_message(void* condition, const char *format, ...)
{
	if (!condition)
	{
		va_list listArguments;
		va_start(listArguments, format);
		LogMgr::instance().log(LogMgr::LOG_ERROR, format, listArguments);
	}
	assert(condition);
}
void assert_with_message(bool condition, const char *format, ...)
{
	if (!condition)
	{
		va_list listArguments;
		va_start(listArguments, format);
		LogMgr::instance().log(LogMgr::LOG_ERROR, format, listArguments);
	}
	assert(condition);
}
#endif