///////////////////////////////////////////////////////////////////////////////////////////////////////
// Copyright (C) 2011 The ZombieWar Engine Open Source Project
// File: LogMgr.h
// Original Author: Salvador Noel Romo Garcia.
//
// Purpose: Tool used for log output
//
///////////////////////////////////////////////////////////////////////////////////////////////////////

#pragma once
#include "LogMgr.h"
#include "Poco/Logger.h"
#ifdef PAKAL_WIN32_PLATFORM
	#include "Poco/WindowsConsoleChannel.h"
#else
	#include "Poco/ConsoleChannel.h"
#endif // PAKAL_WIN32_PLATFORM

using namespace Poco;

const unsigned LOGGER_MAX_ENTRY_SIZE = 255;

namespace Pakal
{
	namespace LogMgr
	{
		char buffer[LOGGER_MAX_ENTRY_SIZE];
		Poco::Mutex mutex;

		std::string Format(const char* format, va_list args)
		{
			// Protect the buffer
			Poco::Mutex::ScopedLock lock(LogMgr::mutex);

			vsnprintf(LogMgr::buffer, LOGGER_MAX_ENTRY_SIZE - 1, format, args);
			LogMgr::buffer[LOGGER_MAX_ENTRY_SIZE - 1] = '\0';
			std::string text = LogMgr::buffer;
			return text;
		}

	}
}

void Pakal::LogMgr::log( int level, const char *format, ... )
{
	Poco::Logger& loggerImpl = Poco::Logger::root();

	// Don't do formatting when this logger filters the message.
	// This prevents unecessary string manipulation.
	if (level <=  loggerImpl.getLevel())
	{
		va_list args;
		va_start(args, format);

		std::string messageText = Format(format, args);

		switch (level)
		{
		case LOG_NONE:
			break;
		case LOG_FATAL:
			loggerImpl.fatal(messageText);
			break;
		case LOG_CRITICAL:
			loggerImpl.critical(messageText);
			break;
		case LOG_ERROR:
			loggerImpl.error(messageText);
			break;
		case LOG_WARNING:
			loggerImpl.warning(messageText);
			break;
		case LOG_NOTICE:
			loggerImpl.notice(messageText);
			break;
		case LOG_INFORMATION:
			loggerImpl.information(messageText);
			break;
		case LOG_DEBUG:
			loggerImpl.debug(messageText);
			break;
		case LOG_TRACE:
			loggerImpl.trace(messageText);
			break;
		default:
			loggerImpl.information(messageText);
			break;
		}
	}
}

void Pakal::LogMgr::setLogLevel( int level )
{
	Poco::Logger& loggerImpl = Poco::Logger::root();
	loggerImpl.setLevel(level);
}

bool Pakal::LogMgr::init()
{
#ifdef PAKAL_WIN32_PLATFORM
	Poco::Logger::root().setChannel( new Poco::WindowsColorConsoleChannel() );
#else
	Poco::Logger::root().setChannel( new Poco::ColorConsoleChannel() );
#endif
	Poco::Logger::root().information("Initializing Pakal LogManager" );
	return true;
}
