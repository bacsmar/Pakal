#pragma once

#include "TextReader.h"
#include <fstream>

namespace pugi
{
	class xml_node;
}

namespace Pakal
{
	class _PAKALExport XmlReader : private TextReader
	{
		void parse(std::istream& stream, Element* root);
		void parse_element(pugi::xml_node* node, Element* element);

	public:

		template <class Type> void read(const char* fileName,const char* name,Type& object)
		{
			ASSERT(fileName != nullptr);
			std::ifstream stream;
			stream.open(fileName);
			read(stream, name, object);
			stream.close();
		}

		template <class Type> void read(std::istream& stream, const char* name,Type& object)
		{
			parse(stream, get_root());
			Archive::value<Type>(name, object);
		}

	};
}
