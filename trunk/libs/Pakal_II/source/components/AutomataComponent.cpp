#include "AutomataComponent.h"

#include "StateMachine.h"
#include "persist/XmlReader.h"
#include "ResourceManager.h"

Pakal::AutomataComponent::~AutomataComponent()
{
	delete m_stateMachine;
}

Pakal::AutomataComponent::AutomataComponent()
{
	m_stateMachine = new StateMachine();
}

void Pakal::AutomataComponent::update()
{
	m_stateMachine->update();
}

bool Pakal::AutomataComponent::load_from_file(const std::string& file)
{
	XmlReader automata_reader;
	auto stream = ResourceManager::instance().open_read_resource(file, false);	
	if (stream->bad())
	{
		LOG_ERROR("[AutomataComponent] can't read %s", file.c_str());
		return false;
	}
	automata_reader.read( *stream, "StateMachine", *m_stateMachine);
	return true;
}