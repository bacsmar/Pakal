///////////////////////////////////////////////////////////////////////////////////////////////////////
// Copyright (C) 2026 The PakalEngine Open Source Project
// File: BgfxCallbacks.cpp
//
// Purpose: bgfx callback implementation - see BgfxCallbacks.h
//
///////////////////////////////////////////////////////////////////////////////////////////////////////

#include "BgfxCallbacks.h"
#include "LogMgr.h"

#include <bimg/bimg.h>
#include <bx/file.h>
#include <bx/error.h>

#include <cstdarg>
#include <utility>
#include <vector>
#include <cstdio>
#include <cstdlib>

namespace Pakal
{
	BgfxCallbacks& BgfxCallbacks::instance()
	{
		static BgfxCallbacks callbacks;
		return callbacks;
	}

	void BgfxCallbacks::fatal(const char* filePath, uint16_t line, bgfx::Fatal::Enum code, const char* str)
	{
		LOG_ERROR("[bgfx] fatal %d at %s:%u: %s", static_cast<int>(code), filePath, line, str ? str : "");
		// bgfx requires the application to stop after a fatal error.
		std::abort();
	}

	void BgfxCallbacks::traceVargs(const char* filePath, uint16_t line, const char* format, va_list argList)
	{
		char message[1024];
		std::vsnprintf(message, sizeof(message), format, argList);
		// Trailing newlines come from bgfx's own messages and would add blank log lines.
		size_t length = std::char_traits<char>::length(message);
		while (length > 0 && (message[length - 1] == '\n' || message[length - 1] == '\r'))
		{
			message[--length] = '\0';
		}
		LOG_DEBUG("[bgfx] %s:%u %s", filePath, line, message);
	}

	void BgfxCallbacks::profilerBegin(const char*, uint32_t, const char*, uint16_t)
	{
	}

	void BgfxCallbacks::profilerBeginLiteral(const char*, uint32_t, const char*, uint16_t)
	{
	}

	void BgfxCallbacks::profilerEnd()
	{
	}

	uint32_t BgfxCallbacks::cacheReadSize(uint64_t)
	{
		// No shader cache: bgfx compiles from the binaries it is given.
		return 0;
	}

	bool BgfxCallbacks::cacheRead(uint64_t, void*, uint32_t)
	{
		return false;
	}

	void BgfxCallbacks::cacheWrite(uint64_t, const void*, uint32_t)
	{
	}

	void BgfxCallbacks::screenShot(const char* filePath, uint32_t width, uint32_t height, uint32_t pitch, bgfx::TextureFormat::Enum format,
		const void* data, uint32_t size, bool yflip)
	{
		// a TGA holds blue, green, red, alpha: a renderer that hands red first has its channels swapped here
		std::vector<uint8_t> swapped;
		if (format == bgfx::TextureFormat::RGBA8)
		{
			swapped.assign(static_cast<const uint8_t*>(data), static_cast<const uint8_t*>(data) + size);
			for (uint32_t y = 0; y < height; ++y)
			{
				uint8_t* row = swapped.data() + size_t(y) * pitch;
				for (uint32_t x = 0; x < width; ++x) std::swap(row[x * 4], row[x * 4 + 2]);
			}
			data = swapped.data();
		}
		else if (format != bgfx::TextureFormat::BGRA8)
		{
			LOG_WARNING("[bgfx] Screenshot: format %d written as BGRA8", int(format));
		}
		bx::FileWriter writer;
		bx::Error err;
		if (!bx::open(&writer, filePath, false, &err))
		{
			LOG_ERROR("[bgfx] Screenshot: unable to open '%s'", filePath);
			return;
		}

		// Writes the raw frame as an uncompressed TGA file.
		const int32_t written = bimg::imageWriteTga(&writer, width, height, pitch, data, false, yflip, &err);
		bx::close(&writer);

		if (written == 0 || !err.isOk())
		{
			LOG_ERROR("[bgfx] Screenshot: failed to write '%s'", filePath);
			return;
		}
		LOG_INFO("[bgfx] Screenshot written to '%s' (%ux%u)", filePath, width, height);
	}

	void BgfxCallbacks::captureBegin(uint32_t, uint32_t, uint32_t, bgfx::TextureFormat::Enum, bool)
	{
	}

	void BgfxCallbacks::captureEnd()
	{
	}

	void BgfxCallbacks::captureFrame(const void*, uint32_t)
	{
	}
}
