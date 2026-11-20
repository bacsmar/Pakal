#include  "InputManager_SFML.h"
#include "LogMgr.h"

#include "components/GamepadComponent.h"
#include <SFML/Window.hpp>
#include "sfml/Keyboard_SFML.h"
#include "sfml/Gamepad_SFML.h"
#include "OSManager.h"
#include "sfml/WindowFocus_SFML.h"
#include <atomic>
#include <algorithm>
#include <chrono>


using namespace Pakal;

namespace
{
	std::atomic<bool> s_focused{ true };   // (a window starts focused; the system says so when it changes)
}

bool Pakal::WindowFocus_SFML::focused() { return s_focused.load(); }

std::mutex& Pakal::JoystickLock_SFML::mutex()
{
	static std::mutex lock;
	return lock;
}
void Pakal::WindowFocus_SFML::set_focused(bool focused) { s_focused.store(focused); }

InputManager_SFML::InputManager_SFML(OSManager* os_manager) : m_os_manager(os_manager)
{
}

void InputManager_SFML::initialize()
{
	// the keys and the pads are read only while the game's window has the focus (see WindowFocus_SFML)
	WindowFocus_SFML::set_focused(true);
	if (m_os_manager)
		m_focus_listener = m_os_manager->event_window_focused.add_listener([](const bool& focused) { WindowFocus_SFML::set_focused(focused); });
	auto keyboard = new Keyboard_SFML();
	m_devices.emplace_back(keyboard);
	m_button_devices.emplace_back(keyboard);

	std::lock_guard<std::mutex> joysticks(JoystickLock_SFML::mutex());
	sf::Joystick::update();

	for (unsigned i = 0; i < sf::Joystick::Count; i++)
	{
		if(sf::Joystick::isConnected(i))
		{
			auto identification = sf::Joystick::getIdentification(i);
			LOG_INFO("[InputManager] joystick found: %s", identification.name.toAnsiString().c_str());
			auto gamepad = new Gamepad_SFML();
			gamepad->set_device_id(i);
			m_devices.emplace_back(gamepad);
			m_button_devices.emplace_back(gamepad);
			m_axis_devices.emplace_back(gamepad);
		}		
	}
	
}

void InputManager_SFML::terminate()
{	
	if (m_os_manager && m_focus_listener) m_os_manager->event_window_focused.remove_listener(m_focus_listener);
	m_focus_listener = 0;
	for(auto device : m_devices)
	{
		delete device;
	}
	m_devices.clear();
	m_button_devices.clear();	
	m_axis_devices.clear();	
	m_touch_devices.clear();
}

IButtonDevice* InputManager_SFML::get_button_device(unsigned id) const
{	
	return id < m_button_devices.size() ? m_button_devices[id] : nullptr;
}

IAxisDevice* InputManager_SFML::get_axis_device(unsigned id) const
{
	// the QA tools' sticks, while they push them, are the first pad's
	if (id == 0)
		if (IAxisDevice* sticks = synthetic_sticks()) return sticks;
	return id < m_axis_devices.size() ? m_axis_devices[id] : nullptr;
}

ITouchDevice* InputManager_SFML::get_touch_device(unsigned id) const
{	
	return id < m_touch_devices.size() ? m_touch_devices[id] : nullptr;
}

void InputManager_SFML::register_component_factories(std::vector<IComponentFactory*>& factories)
{	
	factories.emplace_back(CreateComponentFactory<GamepadComponent, GamepadComponent>(this));
}

sf::Sensor::Type pakal_sensor_SFML(InputManager_SFML::Sensors sensor)
{	
	switch (sensor)
	{
	case InputManager_SFML:: Sensors::Accelerometer:
		return sf::Sensor::Accelerometer;
	case InputManager_SFML:: Sensors::Gyroscope:
		return sf::Sensor::Gyroscope;
	case InputManager_SFML:: Sensors::Magnetometer:
		return sf::Sensor::Magnetometer;
	case InputManager_SFML:: Sensors::Gravity:
		return sf::Sensor::Gravity;
	case InputManager_SFML:: Sensors::UserAcceleration:
		return sf::Sensor::UserAcceleration;
	case InputManager_SFML:: Sensors::Orientation:
		return sf::Sensor::Orientation;
	default: break;
	}
	return { sf::Sensor::Gravity };
}

tmath::vector3df InputManager_SFML::get_sensor_value(Sensors sensor) const
{	
	auto s = pakal_sensor_SFML(sensor);
	auto v = sf::Sensor::getValue(s);
	return{ v.x, v.y, v.z };
}

bool InputManager_SFML::is_sensor_available(Sensors sensor) const
{
	return sf::Sensor::isAvailable(pakal_sensor_SFML(sensor));
}

void InputManager_SFML::set_sensor_enabled(Sensors sensor, bool value)
{
	sf::Sensor::setEnabled(pakal_sensor_SFML(sensor), value);
}

tmath::vector2di InputManager_SFML::get_mouse_position() const
{
	auto pos = sf::Mouse::getPosition();
	return{ pos.x, pos.y };
}

// What each pad slot holds now, a bit for each PadInput (a joystick takes the next slot the first time it presses something; past the last
// slot, pads share it). SFML reads the pads through DirectInput on Windows (an Xbox's too: its triggers come together as one axis) and the
// system's joystick files on Linux. The buttons are the usual order of a standard pad (A, B, X, Y, the bumpers, back, start, the sticks' clicks)
void InputManager_SFML::poll_pads()
{
	const long long now = std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::steady_clock::now().time_since_epoch()).count();
	if (now - m_pad_polled_at < 4) return;
	m_pad_polled_at = now;
	for (unsigned& slot : m_pad_state) slot = 0;
	if (m_pad_bindings.empty()) return;
	std::lock_guard<std::mutex> joysticks(JoystickLock_SFML::mutex());
	sf::Joystick::update();
	const float dead = 50.0f;
	for (unsigned pad = 0; pad < sf::Joystick::Count && pad < 8; ++pad)
	{
		if (!sf::Joystick::isConnected(pad)) continue;
		unsigned state = 0;
		auto set = [&state](PadInput input) { state |= 1u << static_cast<unsigned>(input); };
		static const PadInput buttons[10] = { PadInput::A, PadInput::B, PadInput::X, PadInput::Y, PadInput::LeftBumper, PadInput::RightBumper, PadInput::Back, PadInput::Start, PadInput::LeftStick, PadInput::RightStick };
		for (unsigned b = 0; b < 10 && b < sf::Joystick::getButtonCount(pad); ++b)
			if (sf::Joystick::isButtonPressed(pad, b)) set(buttons[b]);
		const float x = sf::Joystick::getAxisPosition(pad, sf::Joystick::X), y = sf::Joystick::getAxisPosition(pad, sf::Joystick::Y);
		const float povX = sf::Joystick::getAxisPosition(pad, sf::Joystick::PovX), povY = sf::Joystick::getAxisPosition(pad, sf::Joystick::PovY);
		if (x < -dead || povX < -dead) set(PadInput::Left);
		if (x > dead || povX > dead) set(PadInput::Right);
		if (y < -dead || povY > dead) set(PadInput::Up);      // (the stick's y grows downward, the hat's grows upward)
		if (y > dead || povY < -dead) set(PadInput::Down);
		const float triggers = sf::Joystick::getAxisPosition(pad, sf::Joystick::Z);   // (an Xbox's two triggers, as one axis: the left adds, the right takes away)
		if (triggers > dead) set(PadInput::LeftTrigger);
		if (triggers < -dead) set(PadInput::RightTrigger);
		if (!state) continue;
		if (m_pad_slot[pad] < 0) m_pad_slot[pad] = std::min(m_pad_slots_given++, kPadSlots - 1);
		m_pad_state[m_pad_slot[pad]] |= state;
	}
}

bool InputManager_SFML::any_pad_pressed()
{
	if (!WindowFocus_SFML::focused()) return false;
	std::lock_guard<std::mutex> joysticks(JoystickLock_SFML::mutex());
	sf::Joystick::update();
	for (unsigned pad = 0; pad < sf::Joystick::Count; ++pad)
	{
		if (!sf::Joystick::isConnected(pad)) continue;
		for (unsigned b = 0; b < sf::Joystick::getButtonCount(pad); ++b)
			if (sf::Joystick::isButtonPressed(pad, b)) return true;
	}
	return false;
}

bool InputManager_SFML::is_key_pressed(Pakal::Key key)
{
	if (synthetic_pressed(key, m_pad_bindings)) return true;   // (the QA tools' keys and pad: whatever window has the focus)
	if (!WindowFocus_SFML::focused()) return false;   // (another window's keys are not the game's)
	auto k = (sf::Keyboard::Key)(key);
	if (sf::Keyboard::isKeyPressed(k)) return true;
	if (m_pad_bindings.empty()) return false;
	poll_pads();
	unsigned any = 0;
	for (unsigned slot : m_pad_state) any |= slot;
	for (const PadBinding& binding : m_pad_bindings)
	{
		const unsigned held = binding.pad < 0 ? any : (binding.pad < kPadSlots ? m_pad_state[binding.pad] : 0u);
		if (binding.key == key && (held & (1u << static_cast<unsigned>(binding.input)))) return true;
	}
	return false;
}
