///////////////////////////////////////////////////////////////////////////////////////////////////////
// Copyright (C) 2026 The PakalEngine Open Source Project
// File: AssimpResourceIO.h
// Purpose: Assimp reading through the resource manager (a folder, a zip, the APK): MeshComponent3D_Bgfx's models and
//          CharacterAssembly's parts
///////////////////////////////////////////////////////////////////////////////////////////////////////

#pragma once
#include "ResourceManager.h"

#include <assimp/IOStream.hpp>
#include <assimp/IOSystem.hpp>

#include <algorithm>
#include <cstring>
#include <filesystem>
#include <string>
#include <vector>

namespace Pakal
{
	// Assimp reads a model and its buffers (a glTF's .bin) through the resource manager: from a folder, a zip or the APK
	class ResourceIOStream final : public Assimp::IOStream
	{
	public:
		explicit ResourceIOStream(SharedPtr<const std::vector<uint8_t>> bytes) : m_bytes(std::move(bytes)) {}
		size_t Read(void* buffer, size_t size, size_t count) override
		{
			if (size == 0) return 0;
			const size_t whole = std::min(count, (m_bytes->size() - m_at) / size);
			std::memcpy(buffer, m_bytes->data() + m_at, whole * size);
			m_at += whole * size;
			return whole;
		}
		size_t Write(const void*, size_t, size_t) override { return 0; }
		aiReturn Seek(size_t offset, aiOrigin origin) override
		{
			const size_t base = origin == aiOrigin_SET ? 0 : origin == aiOrigin_CUR ? m_at : m_bytes->size();
			if (base + offset > m_bytes->size()) return aiReturn_FAILURE;
			m_at = base + offset;
			return aiReturn_SUCCESS;
		}
		size_t Tell() const override { return m_at; }
		size_t FileSize() const override { return m_bytes->size(); }
		void Flush() override {}
	private:
		SharedPtr<const std::vector<uint8_t>> m_bytes;
		size_t m_at = 0;
	};

	class ResourceIOSystem final : public Assimp::IOSystem
	{
	public:
		bool Exists(const char* file) const override { return ResourceMgr.exists(normal(file)); }
		char getOsSeparator() const override { return '/'; }
		Assimp::IOStream* Open(const char* file, const char* mode) override
		{
			if (mode && (std::strchr(mode, 'w') || std::strchr(mode, 'a'))) return nullptr;   // models are only read
			auto bytes = ResourceMgr.read_bytes(normal(file));
			return bytes ? new ResourceIOStream(std::move(bytes)) : nullptr;
		}
		void Close(Assimp::IOStream* stream) override { delete stream; }
	private:
		static std::string normal(const char* file) { return std::filesystem::path(file).lexically_normal().generic_string(); }
	};
}
