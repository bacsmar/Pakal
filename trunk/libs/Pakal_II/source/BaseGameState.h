#pragma once

#include <Poco/BasicEvent.h>
#include <Poco/Delegate.h>

namespace Pakal
{
	class Engine;
	class GameStateSystem;

	class BaseGameState
	{
		friend GameStateSystem;

	public:
		const char* get_name()
		{
			return m_name.c_str();
		}

	private:
		GameStateSystem* game_state_system;
		bool deallocate_on_pop;
		std::string m_name;

	protected:
		Poco::BasicEvent<void> OnResume;
		Poco::BasicEvent<void> OnPause;

		BaseGameState(std::string name): game_state_system(nullptr), deallocate_on_pop(false)
		{
			m_name = name;
		}

		~BaseGameState() { }

		const GameStateSystem* get_game_state_system()
		{
			return game_state_system;
		};

		virtual void initialize(Engine* engine) = 0;

		virtual void terminate() { };
	};

	class SampleState : public BaseGameState
	{

	public:
		explicit SampleState(const std::string& name)
			: BaseGameState(name) { }

	protected:
		void initialize(Engine*) override
		{
			std::cout << "initialize the state " << this->get_name() << std::endl;
			this->OnResume += Poco::Delegate<SampleState, void>(this, &SampleState::ResumeMenu);
			this->OnPause += Poco::Delegate<SampleState, void>(this, &SampleState::PauseMenu);
		}

		void terminate() override 
		{
			std::cout << "on terminate " << get_name() << std::endl;
		}

	public:
		virtual ~SampleState() {}

		void ResumeMenu(const void* sender)
		{
			std::cout << "on Resume " << get_name() << std::endl;
		};

		void PauseMenu(const void* sender)
		{
			std::cout << "on Pause " << get_name() << std::endl;
		};
	};

}