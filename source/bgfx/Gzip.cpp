///////////////////////////////////////////////////////////////////////////////////////////////////////
// Copyright (C) 2026 The PakalEngine Open Source Project
// File: Gzip.cpp
// Purpose: Bytes compressed as a gzip file (see Gzip.h)
///////////////////////////////////////////////////////////////////////////////////////////////////////

#include "Gzip.h"

#include <miniz.h>

#include <cstdlib>

namespace Pakal
{
	std::string gzip(const std::string& data, int level)
	{
		// the deflate stream alone (no zlib header: negative window bits), then the gzip frame around it
		const int flags = static_cast<int>(tdefl_create_comp_flags_from_zip_params(level < 1 ? 1 : level > 9 ? 9 : level, -MZ_DEFAULT_WINDOW_BITS, MZ_DEFAULT_STRATEGY));
		size_t size = 0;
		void* packed = tdefl_compress_mem_to_heap(data.data(), data.size(), &size, flags);
		if (!packed && !data.empty()) return std::string();
		std::string out;
		out.reserve(size + 18);
		// the header: the magic, deflate, no flags, no time, no extra flags, an unknown system
		static const unsigned char head[10] = { 0x1f, 0x8b, 8, 0, 0, 0, 0, 0, 0, 0xff };
		out.append(reinterpret_cast<const char*>(head), sizeof(head));
		if (packed) out.append(static_cast<const char*>(packed), size);
		std::free(packed);
		// the trailer: the CRC-32 of the data and its size, both little-endian
		const mz_ulong crc = mz_crc32(MZ_CRC32_INIT, reinterpret_cast<const unsigned char*>(data.data()), data.size());
		const unsigned long length = static_cast<unsigned long>(data.size());
		for (int i = 0; i < 4; ++i) out.push_back(static_cast<char>((crc >> (8 * i)) & 0xff));
		for (int i = 0; i < 4; ++i) out.push_back(static_cast<char>((length >> (8 * i)) & 0xff));
		return out;
	}
}
