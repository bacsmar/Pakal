namespace Pakal
{
	template<class T, class WrapperType>
	class CppBindClass
	{
		friend class ScriptComponentLua;
		WrapperType m_wrapper;
		
		explicit CppBindClass(const decltype(m_wrapper)& wrapper)
			: m_wrapper(wrapper){}

		static CppBindClass<T, WrapperType> bind(const decltype(m_wrapper)& wrapper)
		{
			return CppBindClass<T, WrapperType>(wrapper);
		}
	public:
		/**
		* Copy constructor.
		*/
		CppBindClass(const CppBindClass& that)
			: m_wrapper(that.m_wrapper)
		{}

		/**
		* Move constructor for temporaries.
		*/
		CppBindClass(CppBindClass&& that)
			: m_wrapper(std::move(that.m_wrapper))
		{}

		/**
		* Copy assignment.
		*/
		CppBindClass<T, WrapperType>& operator = (const CppBindClass<T, WrapperType>& that)
		{
			m_wrapper = that.m_wrapper;
			return *this;
		}

		/**
		* Move assignment for temporaries.
		*/
		CppBindClass<T, WrapperType>& operator = (CppBindClass<T, WrapperType>&& that)
		{
			m_wrapper = std::move(that.m_wrapper);
			return *this;
		}

		template <typename V>
		inline CppBindClass<T, WrapperType>& add_constant(const char* name, const V& v)
		{
			m_wrapper.addConstant(name, v);
			return *this;
		}

		/**
		* Add or replace a static non-const data member.
		* The value return to lua is pass-by-value, that will create a local copy in lua.
		* This is different from add_static_variable_ref, which is pass-by-reference, and allow direct access to the variable.
		* This apply only to the class type, the primitive types are always pass-by-value.
		*/
		template <typename V>
		inline CppBindClass<T, WrapperType>& add_static_variable(const char* name, V* v, bool writable = true)
		{
			m_wrapper.addStaticVariable(name, v, writable);
			return *this;
		}

		/**
		* Add or replace a static const data member.
		* The value return to lua is pass-by-value, that will create a local copy in lua.
		* This is different from add_static_variable_ref, which is pass-by-reference, and allow direct access to the variable.
		* This apply only to the class type, the primitive types are always pass-by-value.
		*/
		template <typename V>
		inline CppBindClass<T, WrapperType>& add_static_variable(const char* name, const V* v)
		{
			m_wrapper.addStaticVariable(name, v);
			return *this;
		}

		/**
		* Add or replace a static non-const data member.
		* The value return to lua is pass-by-reference, and allow direct access to the variable.
		* This is different from add_static_variable, which is pass-by-value, and will create a local copy upon access.
		* This apply only to the class type, the primitive types are always pass-by-value.
		*/
		template <typename V>
		typename std::enable_if<std::is_copy_assignable<V>::value, CppBindClass<T, WrapperType>&>::type
			add_static_variable_ref(const char* name, V* v, bool writable = true)
		{
			m_wrapper.addStaticVariableRef(name, v, writable);
			return *this;
		}

		/**
		* Add or replace a static non-const data member.
		* The value return to lua is pass-by-reference, and allow direct access to the variable.
		* This is different from add_static_variable, which is pass-by-value, and will create a local copy upon access.
		* This apply only to the class type, the primitive types are always pass-by-value.
		*/
		template <typename V>
		typename std::enable_if<!std::is_copy_assignable<V>::value, CppBindClass<T, WrapperType>&>::type
			add_static_variable_ref(const char* name, V* v)
		{
			m_wrapper.addStaticVariableRef(name, v);
			return *this;
		}

		/**
		* Add or replace a static const data member.
		* The value return to lua is pass-by-reference, and allow direct access to the variable.
		* This is different from add_static_variable, which is pass-by-value, and will create a local copy upon access.
		* This apply only to the class type, the primitive types are always pass-by-value.
		*/
		template <typename V>
		inline CppBindClass<T, WrapperType>& add_static_variable_ref(const char* name, const V* v)
		{
			m_wrapper.addStaticVariableRef(name, v);
			return *this;
		}

		/**
		* Add or replace a read-write property.
		*/
		template <typename FG, typename FS>
		inline CppBindClass<T, WrapperType>& add_static_property(const char* name, const FG& get, const FS& set)
		{
			m_wrapper.addStaticProperty(name, get, set);
			return *this;
		}

		/**
		* Add or replace a read-only property.
		*/
		template <typename FN>
		inline CppBindClass<T, WrapperType>& add_static_property(const char* name, const FN& get)
		{
			m_wrapper.addStaticProperty(name, get);
			return *this;
		}

		/**
		* Add or replace a static member function.
		*/
		template <typename FN>
		inline CppBindClass<T, WrapperType>& add_static_function(const char* name, const FN& proc)
		{
			m_wrapper.addStaticFunction(name, proc);
			return *this;
		}

		/**
		* Add or replace a static member function, user can specify augument spec.
		*/
		template <typename FN, typename ARGS>
		inline CppBindClass<T, WrapperType>& add_static_function(const char* name, const FN& proc, ARGS)
		{
			m_wrapper.template addStaticFunction<FN, ARGS>(name, proc);
			return *this;
		}

		/**
		* Add or replace a constructor function. Argument spec is needed to match the constructor:
		*
		* add_constructor(LUA_ARGS(int, int))
		*
		* The constructor is invoked when calling the class type table
		* like a function. You can have only one constructor or factory function for a lua class.
		*
		* The template parameter should matches the desired Constructor
		*/
		template <typename ARGS>
		inline CppBindClass<T, WrapperType>& add_constructor(ARGS)
		{
			m_wrapper.template addConstructor<ARGS>();
			return *this;
		}

		/**
		* Add or replace a constructor function, the object is stored via the given SP container.
		* The SP class is ususally a shared pointer class. Argument spec is needed to match the constructor:
		*
		* add_constructor(LUA_SP(std::shared_ptr<OBJ>), LUA_ARGS(int, int))
		*
		* The constructor is invoked when calling the class type table
		* like a function. You can have only one constructor or factory function for a lua class.
		*/
		template <typename SP, typename ARGS>
		inline CppBindClass<T, WrapperType>& add_constructor(SP*, ARGS)
		{
			m_wrapper.template addConstructor<SP*,ARGS>();
			return *this;
		}

		/**
		* Add or replace a constructor function, with custom deleter.
		* You can use LUA_DEL macro to specify deleter. For example, MyClass with release() function
		* and MyClassDeleter as deleter:
		*
		* class MyClass
		* {
		* public:
		*      MyClass(int, int);
		*      void release();
		* };
		*
		* struct MyClassDeleter
		* {
		*     void operator () (MyClass* p)
		*     {
		*         p->release();
		*     }
		* };
		*
		* add_constructor(LUA_DEL(MyClassDeleter), LUA_ARGS(int, int))
		*
		* The constructor is invoked when calling the class type table
		* like a function. You can have only one constructor or factory function for a lua class.
		*/
		template <typename DEL, typename ARGS>
		inline CppBindClass<T, WrapperType>& add_constructor(DEL**, ARGS)
		{
			m_wrapper.template addConstructor<DEL**, ARGS>();
			return *this;
		}

		/**
		* Add or replace a factory function, that is a normal/static function that
		* return the object, pointer or smart pointer of the type:
		*
		* static std::shared_ptr<OBJ> create_obj(ARG1_TYPE arg1, ARG2_TYPE arg2);
		*
		* add_factory(&create_obj)
		*
		* The factory function is invoked when calling the class type table
		* like a function. You can have only one constructor or factory function for a lua class.
		*/
		template <typename FN>
		inline CppBindClass<T, WrapperType>& add_factory(const FN& proc)
		{
			m_wrapper.addFactory(proc);
			return *this;
		}

		/**
		* Add or replace a factory function, that is a normal/static function that
		* return the object, pointer or smart pointer of the type:
		*
		* static std::shared_ptr<OBJ> create_obj(ARG1_TYPE arg1, ARG2_TYPE arg2);
		*
		* add_factory(&create_obj, LUA_ARGS(ARG1_TYPE, _opt<ARG2_TYPE>))
		*
		* The factory function is invoked when calling the class type table
		* like a function. You can have only one constructor or factory function for a lua class.
		*/
		template <typename FN, typename ARGS>
		inline CppBindClass<T, WrapperType>& add_factory(const FN& proc, ARGS)
		{
			m_wrapper.template addFactory<FN, ARGS>(proc);
			return *this;
		}

		/**
		* Add or replace a non-const data member.
		* The value return to lua is pass-by-value, that will create a local copy in lua.
		* This is different from add_variable_ref, which is pass-by-reference, and allow direct access to the variable.
		* This apply only to the class type, the primitive types are always pass-by-value.
		*/
		template <typename V>
		inline CppBindClass<T, WrapperType>& add_variable(const char* name, V T::* v, bool writable = true)
		{
			m_wrapper.addVariable(name, v, writable);
			return *this;
		}

		/**
		* Add or replace a const read-only data member.
		* The value return to lua is pass-by-value, that will create a local copy in lua.
		* This is different from add_variable_ref, which is pass-by-reference, and allow direct access to the variable.
		* This apply only to the class type, the primitive types are always pass-by-value.
		*/
		template <typename V>
		inline CppBindClass<T, WrapperType>& add_variable(const char* name, const V T::* v)
		{
			m_wrapper.addVariable(name, v);
			return *this;
		}

		/**
		* Add or replace a non-const data member.
		* The value return to lua is pass-by-reference, and allow direct access to the variable.
		* This is different from add_variable, which is pass-by-value, and will create a local copy upon access.
		* This apply only to the class type, the primitive types are always pass-by-value.
		*/
		template <typename V>
		typename std::enable_if<std::is_copy_assignable<V>::value, CppBindClass<T, WrapperType>&>::type
			add_variable_ref(const char* name, V T::* v, bool writable = true)
		{
			m_wrapper.addVariableRef(name, v, writable);
			return *this;
		}

		/**
		* Add or replace a non-const data member.
		* The value return to lua is pass-by-reference, and allow direct access to the variable.
		* This is different from add_variable, which is pass-by-value, and will create a local copy upon access.
		* This apply only to the class type, the primitive types are always pass-by-value.
		*/
		template <typename V>
		typename std::enable_if<!std::is_copy_assignable<V>::value, CppBindClass<T, WrapperType>&>::type
			add_variable_ref(const char* name, V T::* v)
		{
			m_wrapper.addVariableRef(name, v);
			return *this;
		}

		/**
		* Add or replace a read-only data member.
		* The value return to lua is pass-by-reference, and allow direct access to the variable.
		* This is different from add_variable, which is pass-by-value, and will create a local copy upon access.
		* This apply only to the class type, the primitive types are always pass-by-value.
		*/
		template <typename V>
		inline CppBindClass<T, WrapperType>& add_variable_ref(const char* name, const V T::* v)
		{
			m_wrapper.addVariableRef(name, v);
			return *this;
		}

		/**
		* Add or replace a property member.
		*/
		template <typename FG, typename FS>
		inline CppBindClass<T, WrapperType>& add_property(const char* name, const FG& get, const FS& set)
		{
			m_wrapper.addProperty(name, get, set);
			return *this;
		}

		/**
		* Add or replace a property member.
		* This overrided function allow you to specify non-const and const version of getter.
		*/
		template <typename FG, typename FGC, typename FS>
		inline CppBindClass<T, WrapperType>& add_property(const char* name, const FG& get, const FGC& get_const, const FS& set)
		{
			m_wrapper.addProperty(name, get, get_const, set);
			return *this;
		}

		/**
		* Add or replace a read-only property member.
		*/
		template <typename FN>
		inline CppBindClass<T, WrapperType>& add_property(const char* name, const FN& get)
		{
			return add_property_read_only(name, get);
		}

		/**
		* Add or replace a read-only property member.
		*/
		template <typename FN>
		inline CppBindClass<T, WrapperType>& add_property_read_only(const char* name, const FN& get)
		{
			m_wrapper.addPropertyReadOnly(name, get);
			return *this;
		}

		/**
		* Add or replace a read-only property member.
		* This overrided function allow you to specify non-const and const version of getter.
		*/
		template <typename FN, typename FNC>
		inline CppBindClass<T, WrapperType>& add_property_read_only(const char* name, const FN& get, const FNC& get_const)
		{
			m_wrapper.addPropertyReadOnly(name, get, get_const);
			return *this;
		}

		/**
		* Add or replace a member function.
		*/
		template <typename FN>
		inline CppBindClass<T, WrapperType>& add_function(const char* name, const FN& proc)
		{
			m_wrapper.addFunction(name, proc);
			return *this;
		}

		/**
		* Add or replace a member function, user can specify augument spec.
		*/
		template <typename FN, typename ARGS>
		inline CppBindClass<T, WrapperType>& add_function(const char* name, const FN& proc, ARGS)
		{
			m_wrapper.template addFunction<FN, ARGS>(name, proc);
			return *this;
		}

		/**
		* Add or replace a meta function, meta function is static function attached to object.
		* It is usually used to implement infix operator for the class, that the first operand may
		* or may not be the object.
		*
		* If the first operand is always the object itself, it is better to use add_function instead.
		*/
		template <typename FN>
		inline CppBindClass<T, WrapperType>& add_meta_function(const char* name, const FN& proc)
		{
			m_wrapper.addMetaFunction(name, proc);
			return *this;
		}

		/**
		* Add or replace a meta function, meta function is static function attached to object.
		* It is usually used to implement infix operator for the class, that the first operand may
		* or may not be the object.
		*
		* If the first operand is always the object itself, it is better to use add_function instead.
		*/
		template <typename FN, typename ARGS>
		inline CppBindClass<T, WrapperType>& add_meta_function(const char* name, const FN& proc, ARGS)
		{
			m_wrapper.template addMetaFunction<FN, ARGS>(name, proc);
			return *this;
		}
	};
}