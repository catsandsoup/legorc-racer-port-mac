// Menus added by the port (title buttons, pause, Settings), drawn in the
// original's art direction and usable with a controller first, then the
// keyboard, then the mouse. See docs/DESIGN_PRINCIPLES.md sections 4-6.
#pragma once

#include <godot_cpp/classes/control.hpp>
#include <godot_cpp/classes/font.hpp>
#include <godot_cpp/classes/input_event.hpp>
#include <godot_cpp/classes/label.hpp>
#include <godot_cpp/classes/panel.hpp>
#include <godot_cpp/classes/texture2d.hpp>

#include <functional>
#include <vector>

namespace ssrc {

using namespace godot;

// Tokens sampled from the original's menus under Wine (DESIGN_PRINCIPLES §4).
namespace ui {
const Color METAL(0.651, 0.651, 0.651); // panels #a6a6a6
const Color METAL_EDGE(0.36, 0.36, 0.36);
const Color PILL(0.569, 0.569, 0.569); // button face #919191
const Color PILL_FOCUS(0.78, 0.80, 0.83);
const Color INK(0.06, 0.06, 0.06); // button labels
const Color TEXT_LIGHT(0.941, 0.941, 0.941); // #f0f0f0
const Color BLUE(0.071, 0.447, 0.749); // #1272bf, focused label
const Color BLUE_DEEP(0.0, 0.173, 0.514); // #002c83
const Color GLOW(0.86, 0.94, 1.0);
const Color OUTLINE(0.47, 0.463, 0.467); // #787677
} // namespace ui

enum class InputDevice { KEYBOARD, XBOX, PLAYSTATION };

Ref<Font> menu_font(); // DejaVu Sans Bold, italicised like Director did
Ref<Texture2D> brushed_metal(); // procedural, in the original's greys

struct MenuItem {
	String label;
	std::function<String()> value; // settings rows: current value text
	std::function<void(int)> change; // settings rows: -1 / +1
	std::function<void()> activate; // buttons
};

class MetalMenu {
public:
	enum Layout { LIST, SETTINGS, BAR };
	// rect in stage coordinates (640x480).
	MetalMenu(Control *parent, const String &title, const Rect2 &rect, Layout layout);
	~MetalMenu();
	void set_items(const std::vector<MenuItem> &items);
	void set_back(std::function<void()> back) { on_back = std::move(back); }
	void set_focus(int i);
	int get_focus() const { return focus; }
	void refresh();
	void set_device(InputDevice d);
	// Returns true when the event was used by the menu.
	bool handle_input(const Ref<InputEvent> &event);
	Control *root = nullptr;

private:
	Layout layout;
	Rect2 rect;
	Panel *panel = nullptr;
	Label *prompts = nullptr;
	std::vector<MenuItem> items;
	std::vector<Panel *> rows;
	std::vector<Label *> labels, values;
	int focus = 0;
	InputDevice device = InputDevice::KEYBOARD;
	std::function<void()> on_back;
	void build_rows();
	void style_row(int i);
	int row_at(const Vector2 &stage_pos) const;
};

} // namespace ssrc
