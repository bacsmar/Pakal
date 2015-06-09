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
/*
std::string Format(const char* format, va_list args)
{
	// Protect the buffer
	Poco::Mutex::ScopedLock lock(this->mutex);

	vsnprintf(Logger::buffer, LOGGER_MAX_ENTRY_SIZE - 1, format, args);
	Logger::buffer[LOGGER_MAX_ENTRY_SIZE - 1] = '\0';
	std::string text = buffer;
	return text;
}

void Log(Level level, const char* format, va_list args)
{
	Poco::Logger& loggerImpl = Poco::Logger::get(name);

	// Don't do formatting when this logger filters the message.
	// This prevents unecessary string manipulation.
	if (level >= (Level) loggerImpl.getLevel())
	{
		std::string messageText = Logger::Format(format, args);
		this->Log(level, messageText);
	}
}

void Logger::Debug(const char* format, ...)
{
	this->Log(LDEBUG, format, args);
}

void ZombieWar::LogMgr::log( int level, const char *format, ... )
{
	va_list args;
	va_start(args, format);

	std::string messageText = Logger::root().format(format, args);
	Poco::Logger::root().information(level, messageText);
}
*/