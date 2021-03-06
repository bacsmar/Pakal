#pragma once

#include "Config.h"

#include "State.h"
#include <set>

namespace Pakal
{
	class ScriptComponent;
	class Archive;

	class _PAKALExport StateMachine
	{
		State *m_current_state = nullptr;
		std::string m_script_file;
		std::set<State*> m_states;
		std::vector<std::string> m_available_commands;
	public:
		StateMachine();
		virtual ~StateMachine(void);
		
		State*			get_state(const std::string& name) const;
		void			set_current_state(const std::string& name);
		inline void		set_current_state(State *s) { m_current_state = s; }
		inline State*	get_current_state() const { return m_current_state; }

		State*	create_state(const std::string& name);
		void	remove_state(State *);
		const decltype(m_available_commands)& get_available_commands() const { return m_available_commands; }

		size_t	size() const;
		// range-based for
		decltype(m_states)::iterator	begin();
		decltype(m_states)::iterator	end();

		void update();
		void process_command(const std::string& command);
		void set_script(ScriptComponent& script);
		virtual void persist(Archive* archive);
	};

}
