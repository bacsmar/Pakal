///////////////////////////////////////////////////////////////////////////////////////////////////////
// Copyright (C) 2026 The PakalEngine Open Source Project
// File: RandomStreams.h
// Purpose: The random numbers of a game from one seed, in named streams that do not touch each other.
//          The streams that decide the game ("sim") are the only ones a replay or a server needs; the
//          ones for how it looks and sounds ("fx", "audio") can draw as often as they like without
//          changing the game. A stream's seed comes from the game's seed and the stream's name, so a
//          new stream does not change the others.
///////////////////////////////////////////////////////////////////////////////////////////////////////

#pragma once

#include <cstdint>
#include <map>
#include <random>
#include <string>
#include <string_view>

namespace Pakal
{
	// One seeded sequence of random numbers
	class RandomStream
	{
	public:
		explicit RandomStream(uint32_t seed = 0) : m_engine(seed) {}

		// A number in [low, high)
		float uniform(float low = 0.0f, float high = 1.0f) { return std::uniform_real_distribution<float>(low, high)(m_engine); }
		// A whole number in [low, high]
		int between(int low, int high) { return std::uniform_int_distribution<int>(low, high)(m_engine); }
		std::mt19937& engine() { return m_engine; }

	private:
		std::mt19937 m_engine;
	};

	class RandomStreams
	{
	public:
		explicit RandomStreams(uint32_t seed = 0) : m_seed(seed) {}

		// The stream of that name, made the first time it is asked for
		RandomStream& operator[](const std::string& name)
		{
			auto found = m_streams.find(name);
			if (found == m_streams.end())
			{
				found = m_streams.emplace(name, RandomStream(seed_for(m_seed, name))).first;
			}
			return found->second;
		}

		uint32_t seed() const { return m_seed; }

		// The seed of a stream: the game's seed mixed with the name (FNV-1a, then a finalizer that spreads nearby seeds)
		static uint32_t seed_for(uint32_t seed, std::string_view name)
		{
			uint32_t hash = 2166136261u;
			for (const char c : name)
			{
				hash = (hash ^ static_cast<uint8_t>(c)) * 16777619u;
			}
			uint32_t x = hash ^ (seed * 0x9E3779B9u);
			x ^= x >> 16;
			x *= 0x7FEB352Du;
			x ^= x >> 15;
			x *= 0x846CA68Bu;
			x ^= x >> 16;
			return x;
		}

	private:
		uint32_t m_seed;
		std::map<std::string, RandomStream> m_streams;
	};
}
