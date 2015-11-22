#pragma once

#include "Config.h"

#include "State.h"
#include <unordered_set>
#include <set>

namespace Pakal
{
	class ScriptComponent;
	class Archive;

	class _PAKALExport StateMachine
	{
		std::set<State*> m_states;
		State *m_current_state = nullptr;
		std::string m_script_file;
	public:
		StateMachine();
		virtual ~StateMachine(void);

		void			set_current_state(const std::string& name);
		inline void		set_current_state(State *s) { m_current_state = s; }
		inline State*	get_current_state() const { return m_current_state; }

		State*	create_state(const std::string& name);
		void	remove_state(State *);

		size_t	size() const;
		// range-based for
		decltype(m_states)::iterator	begin();
		decltype(m_states)::iterator	end();

		void update();
		void process_command(unsigned command);
		void set_script(ScriptComponent& script);
		virtual void persist(Archive* archive);
	};

}
