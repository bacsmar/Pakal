#pragma once
#include "Config.h"
#include <string>
#include <type_traits>
#include "Utils.h"
#include <iterator>


namespace Pakal
{
	enum class _PAKALExport ArchiveType
	{
		Reader,
		Writer,
	};

	class _PAKALExport Archive
	{
		const ArchiveType m_type;

		Archive(const Archive& other) = delete;
		Archive& operator=(const Archive& other) = delete;

		
		template<typename, typename T>
		struct has_persist 
		{
			static_assert(std::integral_constant<T, false>::value,"Second template parameter needs to be of function type.");
		};

		template<typename C, typename Ret, typename... Args>
		struct has_persist<C, Ret(Args...)> 
		{
		private:
			template<typename T>
			static constexpr auto check(T*)
				-> typename
				std::is_same<
				decltype( std::declval<T>().persist(std::declval<Args>()...)),
				Ret    // ^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^
				>::type;  // attempt to call it and see if the return type is correct

			template<typename>
			static constexpr std::false_type check(...);

			typedef decltype(check<C>(0)) type;

		public:
			static constexpr bool value = type::value;
		};

		template< typename C, typename std::enable_if< !trait_utils::has_reserve< C >::value>::type* = nullptr >
		void try_reserve(C&, std::size_t) {}
		template< typename C, typename = std::enable_if_t<trait_utils::has_reserve< C >::value>> 
		void try_reserve(C& c, std::size_t n)
		{
			c.reserve(c.size() + n);
		}

		template< typename C, typename std::enable_if< !trait_utils::is_associative_container<C>::value>::type* = nullptr >
		void optional_repare(C&) {}
		template< typename C, typename = std::enable_if_t<trait_utils::is_associative_container< C >::value>>
		void optional_repare(C& container);

		template<class T>
		void Archive::container_value(const char* name, T& object);

		template<class C, typename std::enable_if<!trait_utils::is_associative_container<C>::value>::type* = nullptr>
		void container_insert_reference(const char* name, C& container);

		template<class C, typename = std::enable_if_t<trait_utils::is_associative_container< C >::value>>
		void container_insert_reference(const char* name, C& container);


	protected:
		virtual void begin_object(const char* name) = 0;
		virtual void end_object_reference(void*& address) = 0;
		virtual void end_object_value(void* address) = 0;
		virtual size_t object_size() = 0;
	public:	

		explicit Archive(ArchiveType type) : m_type(type) { }
		virtual ~Archive() {}

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
		template<class T, typename std::enable_if<has_persist<T,void(Archive*)>::value>::type* = nullptr>
		void value(const char* name, T& object);

		//for an enum, is treated as an int
		template<class T>
		void value(const char* name, T& object, typename std::enable_if<std::is_enum<T>::value>::type* = nullptr);

		//for stl container or array with default childName to "item"
		template<class T, typename = std::enable_if_t<has_persist<T, void(Archive*)>::value == false && std::is_enum<T>::value == false> >
		void value(const char* name, T& object);

		//for stl container that is not associative
		template <template<typename ...> class stl_container, typename T,typename...etc, typename = std::enable_if_t<!trait_utils::iterates_with_pair<stl_container<T,etc...>>::value>>
		void value(const char* name, const char* childName, stl_container<T, etc...>& container);
		
		//for an associative stl container like a map
		template <template<typename ...> class stl_container, typename Key,typename Value,typename...etc, typename std::enable_if<trait_utils::iterates_with_pair<stl_container<Key,Value,etc...>>::value>::type* = nullptr>
		void value(const char* name, const char* childName, stl_container<Key,Value,etc...>& container);

		//for an array 
		template <class T, size_t Length>
		void value(const char* name, const char* childName, T (&container)[Length]);


		//---------Pointers are treated as references------------------------------

		//for a pointer to an object just store the address
		template <class T> 
		void value(const char* name, T*& object);

		//for stl container of pointers just store the addresses
		template <template<typename ...> class stl_container, typename T, typename...etc, typename = std::enable_if_t<!trait_utils::iterates_with_pair<stl_container<T, etc...>>::value>>
		void value(const char* name, const char* childName, stl_container<T*,etc...>& container);

		//for associative stl container just store the addresses of the value field, 
		template <template<typename ...> class stl_container,typename Key,typename Value,typename...etc, typename  std::enable_if<trait_utils::iterates_with_pair<stl_container<Key,Value,etc...>>::value>::type* = nullptr>
		void value(const char* name, const char* childName, stl_container<Key,Value*,etc...>& container);

		//for an array of pointers, just store the adresses
		template <class T, size_t Length> 
		void value(const char* name, const char* childName, T*(&values)[Length]);

	};

	template<class T>
	void Archive::value(const char* name, T*& object)
	{
		begin_object(name);
		end_object_reference(*reinterpret_cast<void**>(static_cast<void*>(&object)));
	}

	template<template <typename ...> class stl_container, typename T, typename ... etc, typename>
	void Archive::value(const char* name, const char* childName, stl_container<T*, etc...>& container)
	{
		switch (m_type)
		{
			case ArchiveType::Reader: 
			{	
				begin_object(name);
					size_t count = object_size();
					try_reserve(container, count);

					for (size_t i = 0; i < count; i++)
					{
						//here I need to delegate the job because sets and vectors work very diferently so I need 2 functions
						container_insert_reference(childName, container);
					}
				end_object_value(&container);
			}
			break;
			case ArchiveType::Writer: 
			{
				begin_object(name);
					for (const T* e : container)
					{
						T* ptr = const_cast<T*>(e);
						
						value(childName,ptr);
					}
				end_object_value(&container);
			} 
			break;
		}
	}

	template<template <typename ...> class stl_container, typename Key, typename Value, typename ... etc, typename std::enable_if<trait_utils::iterates_with_pair<stl_container<Key, Value, etc...>>::value>::type*>
	void Archive::value(const char* name, const char* childName, stl_container<Key, Value*, etc...>& container)
	{
		static_assert(!std::is_pointer<Key>::value, "pointers are not currently supported as key on a map");

		switch (m_type)
		{
			case ArchiveType::Reader: 
			{
				begin_object(name);
					size_t count = object_size();
					for (size_t i = 0; i < count; i++)
					{
						begin_object(childName);

							Key&& key = Key();
							value("key", key);

							auto& pair = *container.insert(std::make_pair(key, nullptr)).first;

							value("value", pair.second);

						end_object_value(&pair);
					}
				end_object_value(&container);
			}
			break;
			case ArchiveType::Writer: 
			{
				begin_object(name);
					for (auto& e : container)
					{
						begin_object(childName);

							Key& fst = const_cast<Key&>(e.first);
							value("key", fst);
							value("value", e.second);

						end_object_value(&e);
					}
				end_object_value(&container);
			}
			break;
		}
	}

	template<class T, size_t Length>
	void Archive::value(const char* name, const char* childName, T*(& values)[Length])
	{
		begin_object(name);
			size_t count = m_type == ArchiveType::Reader ? (object_size() > Length ? Length : object_size()) : Length;
			for (size_t i = 0; i < count; i++)
			{
				value(childName, values[i]);
			}
		end_object_value(&values);
	}

	template<class T, typename std::enable_if< Archive::has_persist<T, void(Archive*)>::value >::type*>
	void Archive::value(const char* name, T& object)
	{
		begin_object(name);
			object.persist(this);
		end_object_value(&object);
	}

	template<class T>
	void Archive::value(const char* name, T& Enum, typename std::enable_if<std::is_enum<T>::value>::type*)
	{
		int n = static_cast<int>(Enum);
		value(name,n);
		Enum = static_cast<T>(n);
	}

	template<class T, typename>
	void Archive::value(const char* name, T& object) 
	{
		static_assert(trait_utils::is_container<T>::value || std::is_array<T>::value, "T/Key must have a persist method or to be a stl container or an array or a pointer or an enum");
		
		value(name, "item", object);
	}

	template<template <typename ...> class stl_container, typename T, typename ... etc, typename>
	void Archive::value(const char* name, const char* childName, stl_container<T, etc...>& container)
	{
		switch (m_type)
		{
			case ArchiveType::Reader: 
			{
				begin_object(name);
					size_t count = object_size();
					try_reserve(container, count);

					for (size_t i = 0; i < count; i++)
					{						
						const T& value = *container.insert(container.end(), T());
						T& ref = const_cast<T&>(value);
						container_value(childName, ref);
					}
				end_object_value(&container);

				//if this is a set then reinsert all the elements because they changed after insertion
				optional_repare(container);
				
			}
			break;
			case ArchiveType::Writer: 
			{
				begin_object(name);
					for (const T& element : container)
					{
						T& e = const_cast<T&>(element);
						container_value(childName, e);
					}
				end_object_value(&container);
			}
			break;
		}	
	}

	template<template <typename ...> class stl_container, typename Key, typename Value, typename ... etc, typename std::enable_if<trait_utils::iterates_with_pair<stl_container<Key,Value,etc...>>::value>::type*>
	void Archive::value(const char* name, const char* childName, stl_container<Key, Value, etc...>& container) 
	{
		static_assert(!std::is_pointer<Key>::value, "pointers are not currently supported as key on a map");

		switch (m_type)
		{
			case ArchiveType::Reader: 
			{
				begin_object(name);
					size_t count = object_size();

					for (size_t i = 0; i < count; i++)
					{
						begin_object(childName);

							Key&& key = Key();
							value("key", key);

							auto& pair = *container.insert(std::make_pair(key, Value())).first;
							
							value("value", pair.second);
							
						end_object_value(&pair);
					}
				end_object_value(&container);
			}
			break;
			case ArchiveType::Writer: 
			{
				begin_object(name);
					for (auto& element : container)
					{
						begin_object(childName);
							Key& fst = const_cast<Key&>(element.first);
							value("key", fst);
							value("value", element.second);
						end_object_value(&element);
					}
				end_object_value(&container);
			}
			break;
		}
	}

	template<class T, size_t Length>
	void Archive::value(const char* name, const char* childName, T(&container)[Length])
	{
		begin_object(name);
		size_t count = m_type == ArchiveType::Reader ? (object_size() > Length ? Length : object_size()) : Length;
		for (size_t i = 0; i < count; i++)
		{
			container_value(childName, container[i]);
		}

		end_object_value(&container);
	}

	template<class T>
	void Archive::container_value(const char* name, T& object) 
	{
		if (has_persist<T, void(Archive*)>::value || trait_utils::is_container<T>::value)
		{
			value(name, object);
		}
		else
		{
			begin_object(name);
				value("value", object);
			end_object_value(&object);
		}
	}

	template<class C, typename std::enable_if<!trait_utils::is_associative_container<C>::value>::type*>
	void Archive::container_insert_reference(const char* name, C& container) 
	{
		auto*& address = *container.insert(container.end(), nullptr);

		value(name, address);
	}

	template<class C, typename>
	void Archive::container_insert_reference(const char* name, C& container) 
	{
		void* resolvedAddress = nullptr;

		value(name, resolvedAddress);

		if (resolvedAddress != nullptr)
		{
			container.insert(container.end(), typename C::key_type(resolvedAddress));
		}
		else
		{
			ASSERT_MSG(false, "is not possible to create an associative container with unresolved references until I receive some donations");
		}
	}

	template<typename C, typename>
	void Archive::optional_repare(C& container)
	{
		C backup;
		backup.insert(container.begin(), container.end());
		container.clear();
		container.insert(backup.begin(), backup.end());
	}

}