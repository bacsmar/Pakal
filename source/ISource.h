#pragma once
#include <cstdint>

#include "Config.h"
#include "RTTI.h"
#include "TaskFwd.h"
#include "PakalPath.h"

namespace Pakal
{

	class _PAKALExport ISource
	{
		DECLARE_RTTI(ISource);
	public:

		virtual ~ISource() {}

		virtual SharedPtr<std::istream> open_resource(const Path& pathToResource) = 0;
		// Whether it has the file, without reading it (a source that can tell cheaply says so; the others open it)
		virtual bool has_resource(const Path& pathToResource) { return open_resource(pathToResource) != nullptr; }
		// A file's bytes seen where they lie, without copying them, for as long as the source lives (a pack kept uncompressed in the
		// APK: mapped by the asset manager). Only a source that can do it says yes; a zip mounted from such a view reads it in place
		// A file's size as stored, without reading it (a zip: the size it opens to, as its directory says; the reading checks it). False:
		// not known here
		virtual bool resource_size(const Path& pathToResource, uint64_t& size) { (void)pathToResource; (void)size; return false; }
		virtual bool map_resource(const Path& pathToResource, const void*& data, size_t& size) { (void)pathToResource; (void)data; (void)size; return false; }
	};
}
