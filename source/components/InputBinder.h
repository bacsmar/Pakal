////////////////////////////////////////////////////////////
//
//
////////////////////////////////////////////////////////////

#pragma once
#include <vector>
#include "PakalMath.h"
#include "persist/Archive.h"
#include "persist/TextReader.h"

namespace Pakal
{
	class InputBinder
	{
		//--- structure definition
		struct BindMapping
		{
			std::string bind_Name;
			std::string command;
			std::string event_Type;

			void persist(Pakal::Archive* archive)
			{
				archive->value("BindName", bind_Name);
				archive->value("Command", command);
				archive->value("EventType", event_Type);
			}
		};//-- end Structure definition
		//--InputDevice Private members
		std::vector<BindMapping> m_bind_Mappings;
	public:

		std::string device_Type;


		void persist(Pakal::Archive* archive)
		{
			archive->value("DeviceType", device_Type);
			archive->value("", "BindMapping", m_bind_Mappings);
		}

		inline void add_frame(std::string& bind_name, std::string& command, std::string& eventType)
		{
			m_bind_Mappings.emplace_back(BindMapping{bind_name,command,eventType});
		}

		inline size_t get_size() const
		{
			return m_bind_Mappings.size();
		}

		inline const BindMapping& get_BindMap(size_t index) const
		{
			return m_bind_Mappings[index];
		}
	};

	//-- Structure definition
	struct BindingMap
	{
		std::string entity_name;
		std::vector<InputBinder> m_inputDevices;


		void persist(Pakal::Archive* archive)
		{
			archive->value("EntityName", entity_name);
			archive->value("", "InputDevice", m_inputDevices);
		}

		bool load(Pakal::TextReader& reader, std::istream& stream)
		{
			auto result = reader.read(stream, "BindingMap", *this);

			return result;
		}
	};
}

