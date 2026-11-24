#include "quests/view/QuestsPanel.h"

#include <dear-imgui/imgui.h>

namespace Pakal::Quests
{
	namespace
	{
		void draw_quest(QuestSystem& quests, const Quest& q, PanelState& state, bool actions)
		{
			const QuestRecord* record = quests.record(q.record);
			const Stage* stage = record ? record->stage(q.stage) : nullptr;
			ImGui::Text("stage %d   %.1f s in it, %.1f s in all", q.stage, q.elapsed, q.total);
			if (stage)
			{
				for (const std::string& line : stage->objectives) ImGui::BulletText("line: %s", line.c_str());
				if (!stage->marker.empty()) ImGui::BulletText("marker: %s", stage->marker.c_str());
				for (const Exit& exit : stage->exits) ImGui::BulletText("waits on: %s -> %d", describe_exit(exit).c_str(), exit.to);
				if (stage->hasWhile) ImGui::BulletText("while: %s", describe_exit(stage->whileCond).c_str());
			}
			for (const auto& [name, ref] : q.aliases)
				ImGui::BulletText("alias %s: %s", name.c_str(), ref.valid() ? (ref.kind + ":" + std::to_string(ref.id)).c_str() : "(empty)");
			for (const auto& [name, value] : q.vars) ImGui::BulletText("var %s = %g", name.c_str(), value);
			if (!q.spawned.empty()) ImGui::BulletText("spawns standing: %zu", q.spawned.size());
			if (q.checkpoint) ImGui::BulletText("checkpoint: stage %d", q.checkpoint);
			if (!actions || q.state != "active" || !record || record->stages.empty()) return;
			// a stuck quest moved: to a stage of its record (logged with cause "qa"), its checkpoint again, abandoned, failed
			std::vector<std::string> names;
			for (const Stage& s : record->stages) names.push_back(std::to_string(s.id) + (s.end.empty() ? "" : " (" + s.end + ")"));
			int& choice = state.stageChoice[q.serial];
			if (choice < 0 || choice >= static_cast<int>(names.size())) choice = 0;
			ImGui::SetNextItemWidth(ImGui::GetFontSize() * 8.0f);
			if (ImGui::BeginCombo("##stage", names[static_cast<size_t>(choice)].c_str()))
			{
				for (size_t i = 0; i < names.size(); ++i)
					if (ImGui::Selectable(names[i].c_str(), static_cast<int>(i) == choice)) choice = static_cast<int>(i);
				ImGui::EndCombo();
			}
			ImGui::SameLine();
			if (ImGui::SmallButton("go to stage")) quests.force_stage(q.serial, record->stages[static_cast<size_t>(choice)].id);
			ImGui::SameLine();
			if (ImGui::SmallButton("restart at checkpoint")) quests.restart_checkpoint(q.serial);
			if (ImGui::SmallButton("abandon")) quests.abandon(q.serial);
			ImGui::SameLine();
			if (ImGui::SmallButton("fail")) quests.fail(q.serial, "the quests panel");
		}
	}

	void draw_quests_panel(QuestSystem& quests, const Arbiter& arbiter, PanelState& state, bool actions)
	{
		const Presentation& shown = arbiter.presentation();
		ImGui::Text("shown: %s", shown.owner.empty() ? "(nothing)" : shown.owner.c_str());
		if (!quests.last_refusal().empty()) ImGui::TextColored(ImVec4(1.0f, 0.7f, 0.3f, 1.0f), "last start refused: %s", quests.last_refusal().c_str());
		int running = 0;
		for (const auto& [serial, q] : quests.quests()) running += q.state == "active" ? 1 : 0;
		ImGui::Text("%d running, %zu kept, %zu records", running, quests.quests().size(), quests.records().size());
		// the player's quest items, and the uses offered now (what the game's contextual button shows)
		std::string items;
		for (const auto& [name, n] : quests.items()) items += (items.empty() ? "" : ", ") + name + (n > 1 ? " x" + std::to_string(n) : std::string());
		ImGui::Text("items: %s", items.empty() ? "(none)" : items.c_str());
		for (const QuestSystem::Offer& o : quests.offers())
			ImGui::Text("  offered: %s %s (quest %llu, stage %d, %.1f m)", o.action.c_str(), o.alias.c_str(), static_cast<unsigned long long>(o.serial), o.stage, o.distance);
		// the running first, then the ended (newest first)
		for (int pass = 0; pass < 2; ++pass)
			for (auto it = quests.quests().rbegin(); it != quests.quests().rend(); ++it)
			{
				const Quest& q = it->second;
				if ((q.state == "active") != (pass == 0)) continue;
				const std::string label = q.owner() + "  [" + q.state + "]";
				if (ImGui::TreeNodeEx(label.c_str(), pass == 0 ? ImGuiTreeNodeFlags_DefaultOpen : 0))
				{
					draw_quest(quests, q, state, actions);
					ImGui::TreePop();
				}
			}
		// what the game refused or found at its load: shown always (a refused record is not among the records below)
		if (!state.loadFindings.empty())
		{
			ImGui::TextColored(ImVec4(1.0f, 0.5f, 0.5f, 1.0f), "at load: %zu finding(s) (a record with one was left out)", state.loadFindings.size());
			for (const std::string& f : state.loadFindings) ImGui::TextColored(ImVec4(1.0f, 0.5f, 0.5f, 1.0f), "  %s", f.c_str());
		}
		if (actions && ImGui::Button("validate the loaded records"))
		{
			state.findings.clear();
			for (const auto& [id, record] : quests.records())
				for (const std::string& f : check_record(record, quests.settings(), {})) state.findings.push_back(f);
			for (const std::string& f : check_chains(quests.records())) state.findings.push_back(f);
			state.validated = true;
		}
		if (state.validated)
		{
			// (the records the system holds, against the settings; the game's texts and rewards were checked at its load)
			if (state.findings.empty())
				ImGui::TextColored(ImVec4(0.5f, 1.0f, 0.5f, 1.0f), state.loadFindings.empty() ? "the loaded records check clean; nothing refused at load"
					: "the loaded records check clean; see the load's findings above");
			for (const std::string& f : state.findings) ImGui::TextColored(ImVec4(1.0f, 0.5f, 0.5f, 1.0f), "%s", f.c_str());
		}
		if (ImGui::TreeNode("last transitions"))
		{
			const auto& log = quests.log();
			const size_t from = log.size() > 20 ? log.size() - 20 : 0;
			for (size_t i = log.size(); i-- > from;)
			{
				const nlohmann::json& e = log[i];
				ImGui::Text("%llu  %s#%llu  %d -> %d  %s  (%s)", static_cast<unsigned long long>(e.value("step", static_cast<uint64_t>(0))), e.value("record", std::string()).c_str(),
					static_cast<unsigned long long>(e.value("serial", static_cast<uint64_t>(0))), e.value("from", 0), e.value("to", 0), e.value("outcome", std::string()).c_str(),
					e.value("cause", std::string()).c_str());
			}
			ImGui::TreePop();
		}
	}
}
