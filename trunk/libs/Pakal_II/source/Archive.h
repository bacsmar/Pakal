#pragma once
#include "Config.h"
#include <string>
#include <type_traits>
#include "Utils.h"
#include <iterator>


#if  !defined(_MSC_VER) || _MSC_VER < 1900
#if __cplusplus <= 201103L
namespace std
{
	template<bool _Test,
	class _Ty = void>
		using enable_if_t = typename enable_if<_Test, _Ty>::type;
}
#endif
#endif

/*
json format, test,
*/


namespace Pakal
{
	enum class _PAKALExport ArchiveType
	{
		Reader,
		Writer,
		Resolver,
	};

	class _PAKALExport Archive
	{
		ArchiveType m_type;

		Archive(const Archive& other) = delete;
		Archive& operator=(const Archive& other) = delete;
		
		template<typename T>
		struct has_persist
		{
			template <class C>
			static char(&f(typename std::enable_if<
				std::is_same<void, decltype(std::declval<C>().persist(std::declval<Archive*>()))>::value, void>::type*))[1];

			template<typename C> static char(&f(...))[2];

			static constexpr bool value = sizeof(f<T>(nullptr)) == 1;
		};

		template< typename C, std::enable_if_t<!trait_utils::has_reserve<C>::value>* = nullptr >
		void try_reserve(C&, std::size_t) {}
		template< typename C, std::enable_if_t<trait_utils::has_reserve<C>::value>* = nullptr> 
		void try_reserve(C& c, std::size_t n)
		{
			c.reserve(c.size() + n);
		}

		template<class T, std::enable_if_t<has_persist<T>::value>* = nullptr>
		void container_value(const T& object);

		template<class T, std::enable_if_t<!has_persist<T>::value>* = nullptr >
		void container_value(const T& object);

	protected:
		virtual void begin_object(const char* name) = 0;
		virtual void end_object_as_reference(void*& address) = 0;
		virtual void end_object_as_value(const void* address) = 0;

		virtual size_t children_name_count(const char* name) = 0;

		inline void set_type(ArchiveType type) { m_type = type; }

		explicit Archive(ArchiveType type) : m_type(type) { }
		virtual ~Archive() {}

	public:	

		inline ArchiveType get_type() { return m_type;  }

		virtual void value(const char* name, bool& value) = 0;
		virtual void value(const char* name, char& value) = 0;
		virtual void value(const char* name, signed char& value) = 0;
		virtual void value(const char* name, unsigned char& value) = 0;
		virtual void value(const char* name, short& value) = 0;
		virtual void value(const char* name, unsigned short& value) = 0;
		virtual void value(const char* name, int& value) = 0;
		virtual void value(const char* name, unsigned int& value) = 0;
		virtual void value(const char* name, long& value) = 0;
		virtual void value(const char* name, unsigned long& value) = 0;
		virtual void value(const char* name, float& value) = 0;
		virtual void value(const char* name, double& value) = 0;
		virtual void value(const char* name, char* value, size_t max) = 0;
		virtual void value(const char* name, std::string& value) = 0;

		//for an object that has a member called persist
		template<class T, std::enable_if_t<has_persist<T>::value>* = nullptr>
		void value(const char* name, T& object);

		//for an enum, is treated as an int
		template<class T, std::enable_if_t<std::is_enum<T>::value>* = nullptr>
		void value(const char* name, T& object);

		//for stl container or array with default childName to "item"
		template<class T, std::enable_if_t<has_persist<T>::value == false && std::is_enum<T>::value == false>* = nullptr >
		void value(const char* name, T& object);

		//for stl container that is not associative
		template <template<typename ...> class stl_container, typename T,typename...etc, std::enable_if_t<!trait_utils::iterates_with_pair<stl_container<T,etc...>>::value>* = nullptr>
		void value(const char* name, const char* childName, stl_container<T, etc...>& container);
		
		//for an associative stl container like a map
		template <template<typename ...> class stl_container, typename Key,typename Value,typename...etc, std::enable_if_t<trait_utils::iterates_with_pair<stl_container<Key,Value,etc...>>::value>* = nullptr>
		void value(const char* name, const char* childName, stl_container<Key,Value,etc...>& container);

		////for an array 
		template <class T, size_t Length>
		void value(const char* name, const char* childName, T (&container)[Length]);

		//----------------Pointers as values--------------------------------------------

		template<class T>
		void value(const char* name, T*& object);

		template <template<typename ...> class stl_container, typename T, typename...etc ,std::enable_if_t<!trait_utils::iterates_with_pair<stl_container<T*, etc...>>::value>* = nullptr>
		void value(const char* name, const char* childName, stl_container<T*, etc...>& container);

		template <template<typename ...> class stl_container, typename Key, typename Value, typename...etc, std::enable_if_t<trait_utils::iterates_with_pair<stl_container<Key, Value*, etc...>>::value>* = nullptr>
		void value(const char* name, const char* childName, stl_container<Key, Value*, etc...>& container);

		template <class T, size_t Length>
		void value(const char* name, const char* childName, T*(&container)[Length]);


		//---------Pointers as references------------------------------

		//for a pointer to an object just store the address
		template <class T> 
		void refer(const char* name, T*& object);

		//for stl container of pointers just store the addresses
		template <template<typename ...> class stl_container, typename T, typename...etc, std::enable_if_t<!trait_utils::iterates_with_pair<stl_container<T*, etc...>>::value>* = nullptr>
		void refer(const char* name, const char* childName, stl_container<T*,etc...>& container);

		//for associative stl container just store the addresses of the value field, 
		template <template<typename ...> class stl_container,typename Key,typename Value,typename...etc, std::enable_if_t<trait_utils::iterates_with_pair<stl_container<Key,Value*,etc...>>::value>* = nullptr>
		void refer(const char* name, const char* childName, stl_container<Key,Value*,etc...>& container);

		//for an array of pointers, just store the adresses
		template <class T, size_t Length> 
		void refer(const char* name, const char* childName, T*(&values)[Length]);

	};

	template<class T, std::enable_if_t<Archive::has_persist<T>::value>*>
	void Archive::value(const char* name, T& object)
	{
		begin_object(name);
			object.persist(this);
		end_object_as_value(&object);
	}

	template<class T, std::enable_if_t<std::is_enum<T>::value>*>
	void Archive::value(const char* name, T& Enum)
	{
		auto n = static_cast<typename std::underlying_type<T>::type>(Enum);
		value(name, n);
		Enum = static_cast<T>(n);
	}

	template<class T, std::enable_if_t<Archive::has_persist<T>::value == false && std::is_enum<T>::value == false>* >
	void Archive::value(const char* name, T& object)
	{
		static_assert(trait_utils::is_container<T>::value || std::is_array<T>::value, "T/Key must have a persist method or to be a stl container or an array or a pointer or an enum");

		value(name, "item", object);
	}

	template<template<typename...> class stl_container, typename T, typename... etc, std::enable_if_t<!trait_utils::iterates_with_pair<stl_container<T, etc...>>::value>*>
	void Archive::value(const char* name, const char* childName, stl_container<T, etc...>& container)
	{
		if (*name != 0)
			begin_object(name);

		switch (m_type)
		{
			case ArchiveType::Reader:
			{
				size_t count = children_name_count(childName);
				try_reserve(container, count);

				for (size_t i = 0; i < count; i++)
				{
					begin_object(childName);
						T&& value = T();
						container_value(value);
					end_object_as_value(&*container.insert(container.end(), value));
				}
			}
			break;
			case ArchiveType::Writer:
			case ArchiveType::Resolver:
			{
				for (const T& element : container)
				{
					begin_object(childName);
						container_value(element);
					end_object_as_value(&element);
				}
			}
			break;
		}

		if (*name != 0)
			end_object_as_value(&container);
	}

	template<template <typename ...> class stl_container, typename Key, typename Value, typename ... etc, std::enable_if_t<trait_utils::iterates_with_pair<stl_container<Key, Value, etc...>>::value>*>
	void Archive::value(const char* name, const char* childName, stl_container<Key, Value, etc...>& container)	
	{
		static_assert(!std::is_pointer<Key>::value, "pointers are not currently supported as key on a map");

		if (*name != 0)
			begin_object(name);

		switch (m_type)
		{
			case ArchiveType::Reader:
			{		
				size_t count = children_name_count(childName);

				for (size_t i = 0; i < count; i++)
				{
					begin_object(childName);
					{
						begin_object("key");
							Key&& key = Key();
							container_value(key);

							auto& addresses = *container.insert(std::make_pair(key, Value())).first;
						end_object_as_value(&addresses.first);

						begin_object("value");
							container_value(addresses.second);
						end_object_as_value(&addresses.second);
					}
					end_object_as_value(nullptr);
				}
			}
			break;
			case ArchiveType::Writer:
			case ArchiveType::Resolver:
			{
				for (auto& element : container)
				{
					begin_object(childName);
					{
						begin_object("key");
							container_value(element.first);
						end_object_as_value(&element.first);

						begin_object("value");
							container_value(element.second);
						end_object_as_value(&element.second);
					}
					end_object_as_value(&element);
				}
			}
			break;
		}

		if (*name != 0)
			end_object_as_value(&container);
	}

	template<class T, size_t Length>
	void Archive::value(const char* name, const char* childName, T(& container)[Length])
	{
		if (*name != 0)
			begin_object(name);

		size_t count = Length;

		if (m_type == ArchiveType::Reader || m_type == ArchiveType::Resolver)
		{
			count = children_name_count(childName) > Length ? Length : children_name_count();
		}

		for (size_t i = 0; i < count; i++)
		{
			begin_object(childName);
				container_value(container[i]);
			end_object_as_value(&container[i]);
		}

		if (*name != 0)
			end_object_as_value(&container);
	}

	template<class T>
	void Archive::value(const char* name, T*& object)
	{
		if (this->m_type == ArchiveType::Reader)
		{
			object = new T();
		}

		value(name, *object);
	}

	template<template <typename ...> class stl_container, typename T, typename ... etc, std::enable_if_t<!trait_utils::iterates_with_pair<stl_container<T*, etc...>>::value>*>
	void Archive::value(const char* name, const char* childName, stl_container<T*, etc...>& container)
	{
		if (*name != 0)
			begin_object(name);

		switch (m_type)
		{
			case ArchiveType::Reader:
			{
				size_t count = children_name_count(childName);
				try_reserve(container, count);

				for (size_t i = 0; i < count; i++)
				{
					begin_object(childName);
						T* object = new T();
						container_value(*object);
						container.insert(container.end(), object);
					end_object_as_value(object);
				}
			}
			break;
			case ArchiveType::Writer:
			case ArchiveType::Resolver:
			{
				for (const T* element : container)
				{
					begin_object(childName);
						container_value(*element);
					end_object_as_value(element);
				}
			}
			break;
		}

		if (*name != 0)
			end_object_as_value(&container);
	}

	template<template <typename ...> class stl_container, typename Key, typename Value, typename ... etc, std::enable_if_t<trait_utils::iterates_with_pair<stl_container<Key, Value*, etc...>>::value>*>
	void Archive::value(const char* name, const char* childName, stl_container<Key, Value*, etc...>& container)
	{
		static_assert(!std::is_pointer<Key>::value, "pointers are not currently supported as key on a map");

		if (*name != 0)
			begin_object(name);

		switch (m_type)
		{
			case ArchiveType::Reader:
			{
				size_t count = children_name_count(childName);

				for (size_t i = 0; i < count; i++)
				{
					begin_object(childName);
					{
						begin_object("key");
							Key&& key = Key();
							container_value(key);

							auto& addresses = *container.insert(std::make_pair(key,new Value())).first;
						end_object_as_value(&addresses.first);

						begin_object("value");
							container_value(*addresses.second);
						end_object_as_value(addresses.second);
					}
					end_object_as_value(nullptr);
				}
			}
			break;
			case ArchiveType::Writer:
			case ArchiveType::Resolver:
			{
				for (auto& element : container)
				{
					begin_object(childName);
					{
						begin_object("key");
							container_value(element.first);
						end_object_as_value(&element.first);

						begin_object("value");
							container_value(*element.second);
						end_object_as_value(element.second);
					}
					end_object_as_value(&element);
				}
			}
			break;
		}

		if (*name != 0)
			end_object_as_value(&container);
	}

	template<class T, size_t Length>
	void Archive::value(const char* name, const char* childName, T*(&container)[Length])
	{
		if (*name != 0)
			begin_object(name);

		size_t count = Length;

		if (m_type == ArchiveType::Reader || m_type == ArchiveType::Resolver)
		{
			count = children_name_count(childName) > Length ? Length : children_name_count();
		}

		switch (m_type)
		{
			case ArchiveType::Reader:
			{
				for (size_t i = 0; i < count; i++)
				{
					begin_object(childName);
						T* object = new T();
						container_value(*object);
						container[i] = object;
					end_object_as_value(object);
				}
			}
			break;
			case ArchiveType::Resolver:
			case ArchiveType::Writer:
			{
				for (size_t i = 0; i < count; i++)
				{
					begin_object(childName);
						container_value(*container[i]);
					end_object_as_value(container[i]);
				}
			}
			break;
		}

		if (*name != 0)
			end_object_as_value(&container);
	}

	template<class T>
	void Archive::refer(const char* name, T*& object)
	{
		begin_object(name);
		end_object_as_reference(*reinterpret_cast<void**>(static_cast<void*>(&object)));
	}

	template<template <typename ...> class stl_container, typename T, typename ... etc, std::enable_if_t<!trait_utils::iterates_with_pair<stl_container<T*, etc...>>::value>*>
	void Archive::refer(const char* name, const char* childName, stl_container<T*, etc...>& container)
	{
		if (*name != 0)
			begin_object(name);

		switch (m_type)
		{
			case ArchiveType::Resolver: 
			{	
				size_t count = children_name_count(childName);
				try_reserve(container, count);

				for (size_t i = 0; i < count; i++)
				{
					T* pointer = nullptr;
					refer(childName, pointer);
					container.insert(container.end(), pointer);
				}
			}
			break;
			case ArchiveType::Writer: 
			{
				for (const T* e : container)
				{
					T* ptr = const_cast<T*>(e);
					refer(childName,ptr);
				}
			} 
			break;
			case ArchiveType::Reader: 
			break;
		}

		if (*name != 0)
			end_object_as_value(&container);

	}

	template<template <typename ...> class stl_container, typename Key, typename Value, typename ... etc, std::enable_if_t<trait_utils::iterates_with_pair<stl_container<Key, Value*, etc...>>::value>*>
	void Archive::refer(const char* name, const char* childName, stl_container<Key, Value*, etc...>& container)
	{
		static_assert(!std::is_pointer<Key>::value, "pointers are not currently supported as key on a map");

		if (*name != 0)
			begin_object(name);

		switch (m_type)
		{
			case ArchiveType::Resolver: 
			{
				size_t count = children_name_count(childName);
				for (size_t i = 0; i < count; i++)
				{
					begin_object(childName);
					{
						begin_object("key");
							Key&& key = Key();
							container_value(key);

							auto& addresses = *container.insert(std::make_pair(key, nullptr)).first;
						end_object_as_value(&addresses.first);

						refer("value", addresses.second);
					}
					end_object_as_value(nullptr);
				}
			}
			break;
			case ArchiveType::Writer: 
			{
				for (auto& e : container)
				{
					begin_object(childName);
					{
						begin_object("key");
							container_value(e.first);
						end_object_as_value(&e.first);

						begin_object("value");
						end_object_as_reference(e.second);
					}
					end_object_as_value(&e);
				}
			}
			break;
			case ArchiveType::Reader:
			break;
		}

		if (*name != 0)
			end_object_as_value(&container);
	}

	template<class T, size_t Length>
	void Archive::refer(const char* name, const char* childName, T*(& container)[Length])
	{
		if (*name != 0)
			begin_object(name);

		size_t count = m_type == ArchiveType::Resolver 
			? (children_name_count() > Length ? Length : children_name_count(childName)) 
			: Length;

		switch (m_type)
		{
			case ArchiveType::Resolver:
			case ArchiveType::Writer:
			{
				for (size_t i = 0; i < count; i++)
				{
					refer(childName, container[i]);
				}
			}
			break;
			case ArchiveType::Reader: 
			break;
		}

		if (*name != 0)
			end_object_as_value(&container);
	}

	template<class T, std::enable_if_t<Archive::has_persist<T>::value>*>
	void Archive::container_value(const T& obj)
	{
		T& object = const_cast<T&>(obj);
		object.persist(this);
	}

	template<class T, std::enable_if_t<!Archive::has_persist<T>::value>*>
	void Archive::container_value(const T& obj)
	{
		T& object = const_cast<T&>(obj);
		value("value", object);
	}

}