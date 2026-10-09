// The application: screens (title, level select, help, loading), the flow
// between them (mainManager / titleManager / selectLevelManager /
// helpManager / fadein / fadeout), settings, the native menu bar, mods and
// first-run import of the player's game files.
#pragma once

#include "driving.h"
#include "settings.h"

#include <godot_cpp/classes/canvas_layer.hpp>
#include <godot_cpp/classes/color_rect.hpp>
#include <godot_cpp/classes/control.hpp>
#include <godot_cpp/classes/input_event.hpp>
#include <godot_cpp/classes/label.hpp>
#include <godot_cpp/classes/node.hpp>
#include <godot_cpp/classes/panel_container.hpp>
#include <godot_cpp/classes/texture_rect.hpp>
#include <godot_cpp/variant/callable.hpp>
#include <functional>
#include <vector>

namespace ssrc {

class SoundSystem;

class SupersonicGame : public Node {
	GDCLASS(SupersonicGame, Node)

public:
	SoundSystem *get_sound() const { return sound; }
	const Settings &settings() const { return prefs; }
	Settings &settings_mut() { return prefs; }
	void apply_settings();

	void unlock_mode(ModeId id);
	bool is_unlocked(ModeId id) const;
	void session_finished();

	// Screens / flow.
	void go_title();
	void go_select_level();
	void go_help(int mode);
	void start_mode(int mode);

	// Mod API (also callable from GDScript mods).
	Node *get_session() const { return session; }
	Dictionary get_tuning() const;
	void reload_mods();

	void _ready() override;
	void _process(double delta) override;
	void _unhandled_input(const Ref<InputEvent> &event) override;
	void _notification(int what);

protected:
	static void _bind_methods();

private:
	enum class Screen { NONE, IMPORT, TITLE, SELECT, HELP, LOADING, DRIVING };
	Screen screen = Screen::NONE;
	Settings prefs;
	bool unlocked[int(ModeId::COUNT)] = { true, false, false, false, false };
	bool ctrl_unlock_all = false;
	bool muted = false;
	SoundSystem *sound = nullptr;
	DrivingSession *session = nullptr;
	int pending_mode = 0;

	// UI
	CanvasLayer *ui_layer = nullptr;
	ColorRect *backdrop = nullptr;
	Control *stage = nullptr; // 640x480, centred
	ColorRect *fader = nullptr;
	Label *fps_label = nullptr;
	PanelContainer *settings_panel = nullptr;
	struct Button4 {
		TextureRect *rect = nullptr;
		String base;
		int mode = 0;
		bool hover = false, pressed = false;
	};
	std::vector<Button4> buttons;

	// Fades (fadein / fadeout scripts: 1000 ms at blend +-1 per 10 ms).
	real_t fade_alpha = 0, fade_dir = 0;
	std::function<void()> fade_done;
	bool input_locked = false;
	void fade_in(std::function<void()> done = nullptr);
	void fade_out(std::function<void()> done);
	uint64_t last_usec = 0;

	void clear_stage();
	TextureRect *add_image(const String &member, bool centered_on_reg = true);
	void layout_stage();
	void update_buttons();
	void build_input_map();

	// Import.
	void show_import(const String &message);
	void try_auto_import();
	void import_from(const String &path);
	void on_file_dialog(bool status, const PackedStringArray &selected, int filter_index);
	String importer_path() const;

	// Settings UI.
	struct Choice {
		String label;
		double value;
	};
	struct Option {
		String section, label, key;
		std::vector<Choice> choices;
	};
	std::vector<Option> options;
	void build_options();
	double get_option(const String &key) const;
	void set_option(const String &key, double value);
	void build_native_menu();
	void refresh_native_menu();
	void on_native_menu(const Variant &tag);
	void toggle_settings_panel();
	static void on_option_selected(int idx, Object *game, const String &key, const Array &values);
	void build_settings_panel();
	RID menu_roots[8];
	std::vector<std::pair<RID, int>> menu_items; // (menu, option index) per radio item
	void run_action(const String &action);

	void load_progress();
	void save_progress() const;
	void load_mods();
};

} // namespace ssrc
