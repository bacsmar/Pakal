#pragma once



#include <iostream>
#include <string>
#include <sstream>
#include <random>
#include <time.h>

namespace Pakal
{
	namespace random_generator
	{
		inline int random_between(int a, int b)
		{
			static std::mt19937 mt_rand(static_cast<unsigned>(time(nullptr)));

			return mt_rand() % b + a;
		}

		inline int random()
		{
			static std::mt19937 mt_rand(static_cast<unsigned>(time(nullptr)));

			return mt_rand();
		}
	}
	namespace crypt_utils
	{
		// https://en.wikipedia.org/wiki/Jenkins_hash_function#one-at-a-time
		inline uint32_t hash_joaat(const char *key, size_t len)
		{
			uint32_t hash, i;
			for (hash = i = 0; i < len; ++i)
			{
				hash += key[i];
				hash += (hash << 10);
				hash ^= (hash >> 6);
			}
			hash += (hash << 3);
			hash ^= (hash >> 11);
			hash += (hash << 15);
			return hash;
		}
		inline uint32_t hash_joaat(const std::string& key)
		{
			return hash_joaat(key.c_str(), key.length());
		}

		// https://en.wikipedia.org/wiki/MurmurHash
		#define ROT32(x, y) ((x << y) | (x >> (32 - y))) // avoid effor		
		inline uint32_t murmur3_32(const char *key, uint32_t len, uint32_t seed) {
			static const uint32_t c1 = 0xcc9e2d51;
			static const uint32_t c2 = 0x1b873593;
			static const uint32_t r1 = 15;
			static const uint32_t r2 = 13;
			static const uint32_t m = 5;
			static const uint32_t n = 0xe6546b64;

			uint32_t hash = seed;

			const int nblocks = len / 4;
			const uint32_t *blocks = (const uint32_t *)key;
			int i;
			uint32_t k;
			for (i = 0; i < nblocks; i++) {
				k = blocks[i];
				k *= c1;
				k = ROT32(k, r1);
				k *= c2;

				hash ^= k;
				hash = ROT32(hash, r2) * m + n;
			}

			const uint8_t *tail = (const uint8_t *)(key + nblocks * 4);
			uint32_t k1 = 0;

			switch (len & 3) {
			case 3:
				k1 ^= tail[2] << 16;
			case 2:
				k1 ^= tail[1] << 8;
			case 1:
				k1 ^= tail[0];

				k1 *= c1;
				k1 = ROT32(k1, r1);
				k1 *= c2;
				hash ^= k1;
			}

			hash ^= len;
			hash ^= (hash >> 16);
			hash *= 0x85ebca6b;
			hash ^= (hash >> 13);
			hash *= 0xc2b2ae35;
			hash ^= (hash >> 16);

			return hash;
		}

		inline uint32_t murmur3_32(const std::string& key, uint32_t seed)
		{
			return murmur3_32(key.c_str(), key.length(), seed);
		}
	}
	namespace file_utils
	{
		inline std::streamoff stream_size(std::istream& stream)
		{
			auto originalPosition = stream.tellg();

			stream.seekg(0, std::ios::end);

			auto size = stream.tellg();

			stream.seekg(originalPosition, std::ios::beg);

			return size;
		}
		inline void read_to_string(std::istream& stream, std::string& out_string)
		{
			out_string.resize( static_cast<size_t>(stream_size(stream)) ); 

			stream.read(&out_string[0], out_string.size());
		}


		template<class FN>
		inline void unroll_stringer(FN &f) { }		

		template<class T, class FN, typename ... Args>
		inline void unroll_stringer(FN &f, const T& t, Args const& ... args)
		{
			f << t << " ";
			unroll_stringer(f, args...);
		}

		template< typename ... Args >
		std::string stringer(Args const& ... args)
		{
			std::ostringstream stream;
			unroll_stringer(stream, args...);

			return stream.str();
		}

	}
	namespace map_utils
	{
		template< typename ContainerT, typename PredicateT >
		void erase_if(ContainerT& items, const PredicateT& predicate)
		{
			for (auto it = items.begin(); it != items.end(); )
			{
				if (predicate(*it))
					it = items.erase(it);
				else
					++it;
			}
		};

		template< typename ContainerT, typename TKey >
		bool contains(ContainerT& items,const TKey& element)
		{
			return items.find(element) != items.end();
		}

		template< typename ContainerT, typename TKey, typename TValue>
		bool try_get(ContainerT& map,const TKey& key,TValue& value)
		{
			auto pos = map.find(key);
			if (pos != map.end())
			{
				value = pos->second;
				return true;
			}

			return false;
		}
	}
	namespace enum_utils
	{
		

#define  BIT_ENUM(x) \
	inline x operator~(const x& lhs) { return (x)~(int)lhs; } \
	inline x operator|(const x& lhs, const x& rhs) { return (x)((int)lhs | (int)lhs); } \
	inline x operator&(const x& lhs, const x& rhs) { return (x)((int)lhs & (int)lhs); } \
	inline x operator^(const x& lhs, const x& rhs) { return (x)((int)lhs ^ (int)lhs); } \
	inline x& operator|= (x& lhs, x rhs) { return (x&)((int&)lhs |= (int)rhs); }  \
	inline x& operator&= (x& lhs, x rhs) { return (x&)((int&)lhs &= (int)rhs); } \
	inline x& operator^= (x& lhs, x rhs) { return (x&)((int&)lhs ^= (int)rhs); } 
		

		template<typename TEnum>
		inline bool has_flag(TEnum& value,TEnum flags)
		{
			return (value & flags) == flags;
		}

		template<typename TEnum>
		inline void set_flag(TEnum& value, TEnum flags)
		{
			value |= flags;
		}

		template<typename TEnum>
		inline void unset_flag(TEnum& value, TEnum flags)
		{
			value &= ~flags;
		}

		

	}

}
