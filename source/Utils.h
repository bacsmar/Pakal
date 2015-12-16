#pragma once

#ifndef PAKAL_UTILS_H_
#	define PAKAL_UTILS_H_

#include <iostream>
#include <string>
#include <sstream>

namespace Pakal
{
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


		template< typename ... Args >
		std::string stringer(Args const& ... args)
		{
			std::ostringstream stream;
			using List = int[];

			List d = { 0, ((void)(stream << args << " "), 0) ... };

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

#define  BIT_ENUM(x) \
	inline x operator~(const x& lhs) { return (x)~(int)lhs; } \
	inline x operator|(const x& lhs, const x& rhs) { return (x)((int)lhs | (int)lhs); } \
	inline x operator&(const x& lhs, const x& rhs) { return (x)((int)lhs & (int)lhs); } \
	inline x operator^(const x& lhs, const x& rhs) { return (x)((int)lhs ^ (int)lhs); } \
	inline x& operator|= (x& lhs, x rhs) { return (x&)((int&)lhs |= (int)rhs); }  \
	inline x& operator&= (x& lhs, x rhs) { return (x&)((int&)lhs &= (int)rhs); } \
	inline x& operator^= (x& lhs, x rhs) { return (x&)((int&)lhs ^= (int)rhs); } 

	namespace enum_utils
	{

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

#endif