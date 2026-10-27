///////////////////////////////////////////////////////////////////////////////////////////////////////
// Copyright (C) 2026 The PakalEngine Open Source Project
// File: BgfxCallbacks.h
//
// Purpose: bgfx callback implementation. Sends bgfx traces and fatal errors to the engine log,
//          and writes screenshots requested with bgfx::requestScreenShot as TGA files.
//
///////////////////////////////////////////////////////////////////////////////////////////////////////

#pragma once
#include <bgfx/bgfx.h>

namespace Pakal
{
	class BgfxCallbacks final : public bgfx::CallbackI
	{
	public:
		// Shared instance: bgfx keeps the pointer for its whole lifetime.
		static BgfxCallbacks& instance();

		void fatal(const char* filePath, uint16_t line, bgfx::Fatal::Enum code, const char* str) override;
		void traceVargs(const char* filePath, uint16_t line, const char* format, va_list argList) override;
		void profilerBegin(const char* name, uint32_t abgr, const char* filePath, uint16_t line) override;
		void profilerBeginLiteral(const char* name, uint32_t abgr, const char* filePath, uint16_t line) override;
		void profilerEnd() override;
		uint32_t cacheReadSize(uint64_t id) override;
		bool cacheRead(uint64_t id, void* data, uint32_t size) override;
		void cacheWrite(uint64_t id, const void* data, uint32_t size) override;
		void screenShot(const char* filePath, uint32_t width, uint32_t height, uint32_t pitch,
			const void* data, uint32_t size, bool yflip) override;
		void captureBegin(uint32_t width, uint32_t height, uint32_t pitch, bgfx::TextureFormat::Enum format, bool yflip) override;
		void captureEnd() override;
		void captureFrame(const void* data, uint32_t size) override;

	private:
		BgfxCallbacks() = default;
	};
}
