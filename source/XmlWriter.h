#pragma once

#include "TextWriter.h"
#include <fstream>

namespace Pakal
{
	class _PAKALExport XmlWriter : public TextWriter
	{
		void write(std::ostream& ostream,const Element* element);
		void write_element(std::ostream& stream,const Element* element, int depth);
		void write_indent(std::ostream& stream, int indent);


	public:
		template <class Type> void write(const char* filename, const char* name, Type& object)
		{
			ASSERT(filename != nullptr);
			std::ofstream stream;
			stream.open(filename);
			write(stream, name, object);
			stream.close();
		}

		template <class Type> void write(std::ostream& stream, const char* name, Type& object)
		{
			Archive::value<Type>(name, object);
			solve_references();
			write(stream, get_root());
		}

	};
}