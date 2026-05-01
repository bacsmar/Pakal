#pragma once
#include "Config.h"
#include "IInputManager.h"
#include "EventArgs.h"

namespace Pakal
{
	/**
	 * @class InputManager_Polling
	 * @brief Convenience wrapper around IInputManager for synchronous key polling.
	 *
	 * This wrapper provides simple polling methods for checking key states without
	 * requiring event subscriptions. It's designed for gameplay code (PlayerController, EnemyAI, etc.)
	 * that needs to check input state directly during update loops.
	 *
	 * Usage:
	 *   auto& input = InputManager_Polling::instance();
	 *   if (input.is_key_pressed(Key::Left)) { ... }
	 */
	class _PAKALExport InputManager_Polling
	{
	public:
		/**
		 * Get the singleton instance of the input polling wrapper.
		 * @return Reference to the global InputManager_Polling instance
		 */
		static InputManager_Polling& instance();

		/**
		 * Initialize the polling wrapper with the underlying IInputManager.
		 * Called once during engine startup after IInputManager is created.
		 * @param input_manager Pointer to the active IInputManager implementation
		 */
		static void initialize(IInputManager* input_manager);

		/**
		 * Check if a keyboard key is currently pressed.
		 * @param key The key code to check (from enum Pakal::Key)
		 * @return true if the key is currently held down, false otherwise
		 */
		bool poll_key_down(Key key) const;

		/**
		 * Check if a specific axis device is available (e.g., joystick analog stick).
		 * @param axis_id Device ID for the axis device
		 * @return Axis value in range [-1.0f, 1.0f] if device exists, 0.0f otherwise
		 */
		float get_axis_value(unsigned axis_id) const;

		/**
		 * Get mouse position in screen coordinates.
		 * @return Vector2D containing (x, y) pixel coordinates
		 */
		tmath::vector2di get_mouse_position() const;

		/**
		 * Convenience method: Check if Left arrow key is pressed.
		 */
		inline bool is_move_left_pressed() const { return poll_key_down(Key::Left) || poll_key_down(Key::A); }

		/**
		 * Convenience method: Check if Right arrow key is pressed.
		 */
		inline bool is_move_right_pressed() const { return poll_key_down(Key::Right) || poll_key_down(Key::D); }

		/**
		 * Convenience method: Check if Up arrow key is pressed.
		 */
		inline bool is_move_up_pressed() const { return poll_key_down(Key::Up) || poll_key_down(Key::W); }

		/**
		 * Convenience method: Check if Down arrow key is pressed.
		 */
		inline bool is_move_down_pressed() const { return poll_key_down(Key::Down) || poll_key_down(Key::S); }

		/**
		 * Convenience method: Check if Space key is pressed (jump/action).
		 */
		inline bool is_jump_pressed() const { return poll_key_down(Key::Space) || poll_key_down(Key::Up) || poll_key_down(Key::W); }

		/**
		 * Convenience method: Check if Z key is pressed (fire weapon).
		 */
		inline bool is_fire_pressed() const { return poll_key_down(Key::Z) || poll_key_down(Key::LControl) || poll_key_down(Key::RControl); }

		/**
		 * Convenience method: Check if X key is pressed (alternate action).
		 */
		inline bool is_action_pressed() const { return poll_key_down(Key::X); }

	private:
		/**
		 * Private constructor for singleton pattern.
		 */
		InputManager_Polling();

		/**
		 * Pointer to the underlying IInputManager implementation.
		 * Set during initialize() call.
		 */
		IInputManager* m_input_manager;

		/**
		 * Static instance for singleton pattern.
		 */
		static InputManager_Polling* ms_instance;
	};
}
