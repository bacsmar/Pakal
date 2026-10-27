#pragma once

#include <cstdint>

namespace Pakal
{
	struct EntityHandle
	{
		uint64_t id = 0;
		uint32_t generation = 0;

		bool is_valid() const { return id != 0; }
		explicit operator bool() const { return is_valid(); }
	};
}