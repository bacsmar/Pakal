///////////////////////////////////////////////////////////////////////////////////////////////////////
// Copyright (C) 2026 The PakalEngine Open Source Project
// File: ByteStream.h
// Purpose: Bytes for the network: a writer and a reader of little-endian numbers, variable-length
//          integers (small ones take one byte), quantized floats (a position in centimeters, an angle in
//          256ths of a turn) and strings. A reader past the end, or given a broken message, stops and says
//          so (ok() is false) instead of reading garbage.
///////////////////////////////////////////////////////////////////////////////////////////////////////

#pragma once
#include <cmath>
#include <cstdint>
#include <cstring>
#include <string>
#include <vector>

namespace Pakal
{
	class ByteWriter
	{
	public:
		void u8(uint8_t v) { m_data.push_back(v); }
		void u16(uint16_t v) { u8(static_cast<uint8_t>(v)); u8(static_cast<uint8_t>(v >> 8)); }
		void u32(uint32_t v) { u16(static_cast<uint16_t>(v)); u16(static_cast<uint16_t>(v >> 16)); }
		void f32(float v) { uint32_t bits; std::memcpy(&bits, &v, 4); u32(bits); }
		// 7 bits a byte: below 128 takes one
		void varuint(uint64_t v)
		{
			while (v >= 0x80)
			{
				u8(static_cast<uint8_t>(v | 0x80));
				v >>= 7;
			}
			u8(static_cast<uint8_t>(v));
		}
		// zigzag: small negative numbers are small too
		void varint(int64_t v) { varuint((static_cast<uint64_t>(v) << 1) ^ static_cast<uint64_t>(v >> 63)); }
		// 'v' in steps of 'step' (a position to the centimeter: 0.01)
		void quantized(float v, float step) { varint(static_cast<int64_t>(std::llround(v / step))); }
		// an angle (radians) in 256ths of a turn
		void angle(float radians)
		{
			const float turn = 6.2831853f;
			float a = std::fmod(radians, turn);
			if (a < 0.0f) a += turn;
			u8(static_cast<uint8_t>(static_cast<int>(std::lround(a / turn * 256.0f)) & 0xff));
		}
		void string(const std::string& s)
		{
			varuint(s.size());
			m_data.insert(m_data.end(), s.begin(), s.end());
		}
		void bytes(const std::vector<uint8_t>& b)
		{
			varuint(b.size());
			m_data.insert(m_data.end(), b.begin(), b.end());
		}

		const std::vector<uint8_t>& data() const { return m_data; }
		std::vector<uint8_t>& data() { return m_data; }
		size_t size() const { return m_data.size(); }

	private:
		std::vector<uint8_t> m_data;
	};

	class ByteReader
	{
	public:
		ByteReader(const uint8_t* data, size_t size) : m_data(data), m_size(size) {}
		explicit ByteReader(const std::vector<uint8_t>& data) : ByteReader(data.data(), data.size()) {}

		bool ok() const { return m_ok; }
		bool done() const { return m_at >= m_size; }
		size_t left() const { return m_ok && m_at < m_size ? m_size - m_at : 0; }

		uint8_t u8()
		{
			if (!m_ok || m_at >= m_size)
			{
				m_ok = false;
				return 0;
			}
			return m_data[m_at++];
		}
		uint16_t u16() { const uint16_t lo = u8(); return static_cast<uint16_t>(lo | (static_cast<uint16_t>(u8()) << 8)); }
		uint32_t u32() { const uint32_t lo = u16(); return lo | (static_cast<uint32_t>(u16()) << 16); }
		float f32() { const uint32_t bits = u32(); float v; std::memcpy(&v, &bits, 4); return v; }
		uint64_t varuint()
		{
			uint64_t v = 0;
			for (int shift = 0; shift < 64; shift += 7)
			{
				const uint8_t b = u8();
				v |= static_cast<uint64_t>(b & 0x7f) << shift;
				if (!(b & 0x80) || !m_ok) return v;
			}
			m_ok = false;   // too long: not a number this writer made
			return 0;
		}
		int64_t varint() { const uint64_t z = varuint(); return static_cast<int64_t>(z >> 1) ^ -static_cast<int64_t>(z & 1); }
		float quantized(float step) { return static_cast<float>(varint()) * step; }
		float angle() { return static_cast<float>(u8()) / 256.0f * 6.2831853f; }
		std::string string()
		{
			const uint64_t length = varuint();
			if (!m_ok || length > left())
			{
				m_ok = false;
				return std::string();
			}
			std::string s(reinterpret_cast<const char*>(m_data + m_at), static_cast<size_t>(length));
			m_at += static_cast<size_t>(length);
			return s;
		}
		std::vector<uint8_t> bytes()
		{
			const uint64_t length = varuint();
			if (!m_ok || length > left())
			{
				m_ok = false;
				return {};
			}
			std::vector<uint8_t> b(m_data + m_at, m_data + m_at + length);
			m_at += static_cast<size_t>(length);
			return b;
		}

	private:
		const uint8_t* m_data;
		size_t m_size;
		size_t m_at = 0;
		bool m_ok = true;
	};
}
