#pragma once


#include <Rocket/Core/FileInterface.h>
#include <unordered_map>
#include <TaskFwd.h>


namespace Pakal
{
	class _PAKALExport PakalRocketFileInterface : public Rocket::Core::FileInterface
	{
	public:
		/** Default constructor */
		explicit PakalRocketFileInterface();
		/** Default destructor */
		virtual ~PakalRocketFileInterface();

		// Opens a file.
		Rocket::Core::FileHandle Open(const Rocket::Core::String& path) override;

		// Closes a previously opened file.
		void Close(Rocket::Core::FileHandle file) override;

		// Reads data from a previously opened file.
		size_t Read(void* buffer, size_t size, Rocket::Core::FileHandle file) override;

		// Seeks to a point in a previously opened file.
		bool Seek(Rocket::Core::FileHandle file, long offset, int origin) override;

		// Returns the current position of the file pointer.
		size_t Tell(Rocket::Core::FileHandle file) override;

	protected:
		std::unordered_map<Rocket::Core::FileHandle,SharedPtr<std::istream>> m_opened_files;
	};
}
