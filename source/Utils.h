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
	}

	namespace trait_utils
	{

		template< typename ... Args >
		std::string stringer(Args const& ... args)
		{
			std::ostringstream stream;
			using List = int[];

			List d = { 0, ((void)(stream << args << " "), 0) ... };

			return stream.str();
		}


		template<typename T>
		struct has_reserve
		{
			template <class C>
			static char(&f(typename std::enable_if<
				std::is_same<void, decltype(std::declval<C>().reserve(std::declval<size_t>()))>::value, void>::type*))[1];

			template<typename C> static char(&f(...))[2];

			static constexpr bool value = sizeof(f<T>(nullptr)) == 1;
		};

		template<typename T>
		struct iterates_with_pair
		{

			template <class C>
			static char(&f(typename std::enable_if<
				std::is_same<typename C::value_type, std::pair<const typename C::key_type, typename C::mapped_type>>::value, void>::type*))[1];

			template<typename C> static char(&f(...))[2];

			static constexpr bool value = sizeof(f<T>(nullptr)) == 1;
		};

		template<typename T>
		struct has_const_iterator
		{
		private:
			typedef char                      yes;
			typedef struct { char array[2]; } no;

			template<typename C> static yes test(typename C::const_iterator*);
			template<typename C> static no  test(...);
		public:
			static const bool value = sizeof(test<T>(0)) == sizeof(yes);
			typedef T type;
		};

		template <typename T>
		struct has_begin_end
		{
			template<typename C> static char(&f(typename std::enable_if<
				std::is_same<decltype(static_cast<typename C::const_iterator(C::*)() const>(&C::begin)),
				typename C::const_iterator(C::*)() const>::value, void>::type*))[1];

			template<typename C> static char(&f(...))[2];

			template<typename C> static char(&g(typename std::enable_if<
				std::is_same<decltype(static_cast<typename C::const_iterator(C::*)() const>(&C::end)),
				typename C::const_iterator(C::*)() const>::value, void>::type*))[1];

			template<typename C> static char(&g(...))[2];

			static bool const beg_value = sizeof(f<T>(0)) == 1;
			static bool const end_value = sizeof(g<T>(0)) == 1;
		};

		template<typename T>
		struct is_container : std::integral_constant < bool, has_const_iterator<T>::value && has_begin_end<T>::beg_value && has_begin_end<T>::end_value && !std::is_same<T, std::string>::value >
		{ };

		template<typename T>
		struct to_void
		{
			typedef void type;
		};

		template <typename Container, typename dummy = void>
		struct is_associative_container : std::false_type {};

		template <typename Container>
		struct is_associative_container
			<Container, typename to_void<typename Container::key_type>::type > : std::true_type {};

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