///////////////////////////////////////////////////////////////////////////////////////////////////////
// Copyright (C) 2026 The PakalEngine Open Source Project
// File: UiPanel.cpp
// Purpose: A game's interface made of one RmlUi document - implementation
///////////////////////////////////////////////////////////////////////////////////////////////////////

#include "topdown/view/UiPanel.h"

#include "IUIManager.h"
#include "LogMgr.h"

#include <algorithm>
#include <cstdio>

namespace Pakal::TopDown
{
	bool UiPanel::load(const std::string& document, const std::string& body)
	{
		if (!m_ui || !m_ui->load_document(m_id, document))
		{
			LOG_ERROR("[UiPanel] %s did not load", document.c_str());
			return false;
		}
		m_loaded = true;
#if defined(__ANDROID__)
		if (!body.empty()) m_ui->set_class(m_id, body, "touch", true);
#else
		(void)body;
#endif
		m_ui->show_document(m_id);
		m_listener = m_ui->add_click_listener(m_id, [this](const std::string& element)
		{
			std::lock_guard<std::mutex> lock(m_mutex);   // the interface's thread
			m_clicks.push_back(element);
		});
		return true;
	}

	void UiPanel::unload()
	{
		if (!m_loaded) return;
		m_loaded = false;
		m_shown.clear();
		m_ui->remove_click_listener(m_listener);
		m_ui->unload_document(m_id);
	}

	bool UiPanel::changed(const std::string& key, const std::string& value)
	{
		auto found = m_shown.find(key);
		if (found != m_shown.end() && found->second == value) return false;
		m_shown[key] = value;
		return true;
	}

	void UiPanel::text(const std::string& element, const std::string& value)
	{
		if (m_loaded && changed("t|" + element, value)) m_ui->set_text(m_id, element, value);
	}

	void UiPanel::markup(const std::string& element, const std::string& value)
	{
		if (!m_loaded || !changed("m|" + element, value)) return;
		// the elements inside are new: what was given to them before does not hold
		const std::string mine = "m|" + element;
		for (auto it = m_shown.begin(); it != m_shown.end();) it = it->first == mine ? std::next(it) : m_shown.erase(it);
		m_ui->set_markup(m_id, element, value);
	}

	void UiPanel::bar(const std::string& element, float share)
	{
		if (!m_loaded) return;
		char width[16];
		std::snprintf(width, sizeof(width), "%.1f%%", 100.0f * std::clamp(share, 0.0f, 1.0f));
		if (changed("p|" + element + "|width", width)) m_ui->set_property(m_id, element, "width", width);
	}

	void UiPanel::property(const std::string& element, const std::string& name, const std::string& value)
	{
		if (m_loaded && changed("p|" + element + "|" + name, value)) m_ui->set_property(m_id, element, name, value);
	}

	void UiPanel::set_class(const std::string& element, const std::string& name, bool on)
	{
		if (m_loaded && changed("c|" + element + "|" + name, on ? "1" : "0")) m_ui->set_class(m_id, element, name, on);
	}

	void UiPanel::attribute(const std::string& element, const std::string& name, const std::string& value)
	{
		if (m_loaded && changed("a|" + element + "|" + name, value)) m_ui->set_attribute(m_id, element, name, value);
	}

	std::vector<std::string> UiPanel::clicks()
	{
		std::lock_guard<std::mutex> lock(m_mutex);
		std::vector<std::string> pressed;
		pressed.swap(m_clicks);
		return pressed;
	}
}
