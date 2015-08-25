#include <mutex>

namespace Pakal
{

	template <class T>
	class SingletonHolder final
	{
	public:
		SingletonHolder():
			_pS(nullptr)
			/// Creates the SingletonHolder.
		{
		}
	
		~SingletonHolder()
			/// Destroys the SingletonHolder and the singleton
			/// object that it holds.
		{
			delete _pS;
		}
	
		T* get()
			/// Returns a pointer to the singleton object
			/// hold by the SingletonHolder. The first call
			/// to get will create the singleton.
		{
			std::lock_guard<std::mutex> lock(_m);
			if (!_pS) _pS = new T;
			return _pS;
		}
	
	private:
		T* _pS;
		std::mutex _m;
	};


} 