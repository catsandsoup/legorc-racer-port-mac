#include "game.h"
#include "assets.h"
#include "sound.h"

#include <godot_cpp/classes/audio_server.hpp>
#include <godot_cpp/classes/button.hpp>
#include <godot_cpp/classes/config_file.hpp>
#include <godot_cpp/classes/dir_access.hpp>
#include <godot_cpp/classes/display_server.hpp>
#include <godot_cpp/classes/engine.hpp>
#include <godot_cpp/classes/file_access.hpp>
#include <godot_cpp/classes/gd_extension_manager.hpp>
#include <godot_cpp/classes/h_box_container.hpp>
#include <godot_cpp/classes/input.hpp>
#include <godot_cpp/classes/input_event_joypad_button.hpp>
#include <godot_cpp/classes/input_event_joypad_motion.hpp>
#include <godot_cpp/classes/input_event_key.hpp>
#include <godot_cpp/classes/input_event_mouse_button.hpp>
#include <godot_cpp/classes/input_event_mouse_motion.hpp>
#include <godot_cpp/classes/input_map.hpp>
#include <godot_cpp/classes/native_menu.hpp>
#include <godot_cpp/classes/option_button.hpp>
#include <godot_cpp/classes/os.hpp>
#include <godot_cpp/classes/project_settings.hpp>
#include <godot_cpp/classes/rendering_server.hpp>
#include <godot_cpp/classes/resource_loader.hpp>
#include <godot_cpp/classes/scene_tree.hpp>
#include <godot_cpp/classes/script.hpp>
#include <godot_cpp/classes/scroll_container.hpp>
#include <godot_cpp/classes/style_box_flat.hpp>
#include <godot_cpp/classes/time.hpp>
#include <godot_cpp/classes/v_box_container.hpp>
#include <godot_cpp/classes/viewport.hpp>
#include <godot_cpp/classes/window.hpp>
#include <godot_cpp/classes/zip_reader.hpp>
#include <godot_cpp/core/class_db.hpp>
#include <godot_cpp/variant/utility_functions.hpp>

namespace ssrc {

static const char *MODE_BUTTONS[] = { "Free", "Checkpoint", "Stunt", "Scavenger", "Supersonic" };

// ============================================================ settings

void Settings::load() {
	Ref<ConfigFile> cf;
	cf.instantiate();
	if (cf->load("user://settings.cfg") != OK) {
		return;
	}
#define LOADV(sec, name) name = decltype(name)(double(cf->get_value(sec, #name, double(name))))
	LOADV("display", window_mode);
	LOADV("display", window_w);
	LOADV("display", window_h);
	LOADV("display", vsync);
	LOADV("display", max_fps);
	LOADV("display", render_scale);
	LOADV("display", upscaler);
	LOADV("display", antialiasing);
	LOADV("display", texture_filter);
	LOADV("display", lighting);
	LOADV("display", fov);
	LOADV("display", camera_mode);
	show_fps = bool(cf->get_value("display", "show_fps", show_fps));
	LOADV("gameplay", game_speed);
	LOADV("gameplay", physics_substeps);
	LOADV("audio", music_volume);
	LOADV("audio", effects_volume);
#undef LOADV
}

void Settings::save() const {
	Ref<ConfigFile> cf;
	cf.instantiate();
#define SAVEV(sec, name) cf->set_value(sec, #name, name)
	SAVEV("display", window_mode);
	SAVEV("display", window_w);
	SAVEV("display", window_h);
	SAVEV("display", vsync);
	SAVEV("display", max_fps);
	SAVEV("display", render_scale);
	SAVEV("display", upscaler);
	SAVEV("display", antialiasing);
	SAVEV("display", texture_filter);
	SAVEV("display", lighting);
	SAVEV("display", fov);
	SAVEV("display", camera_mode);
	SAVEV("display", show_fps);
	SAVEV("gameplay", game_speed);
	SAVEV("gameplay", physics_substeps);
	SAVEV("audio", music_volume);
	SAVEV("audio", effects_volume);
#undef SAVEV
	cf->save("user://settings.cfg");
}

// ============================================================ setup

void SupersonicGame::_bind_methods() {
	ClassDB::bind_method(D_METHOD("session_finished"), &SupersonicGame::session_finished);
	ClassDB::bind_method(D_METHOD("go_title"), &SupersonicGame::go_title);
	ClassDB::bind_method(D_METHOD("go_select_level"), &SupersonicGame::go_select_level);
	ClassDB::bind_method(D_METHOD("start_mode", "mode"), &SupersonicGame::start_mode);
	ClassDB::bind_method(D_METHOD("get_session"), &SupersonicGame::get_session);
	ClassDB::bind_method(D_METHOD("get_tuning"), &SupersonicGame::get_tuning);
	ClassDB::bind_method(D_METHOD("reload_mods"), &SupersonicGame::reload_mods);
	ClassDB::bind_method(D_METHOD("on_native_menu", "tag"), &SupersonicGame::on_native_menu);
	ClassDB::bind_method(D_METHOD("on_file_dialog", "status", "selected", "filter_index"), &SupersonicGame::on_file_dialog);
	ClassDB::bind_method(D_METHOD("import_from", "path"), &SupersonicGame::import_from);
	ClassDB::bind_method(D_METHOD("run_action", "action"), &SupersonicGame::run_action);
	ClassDB::bind_method(D_METHOD("set_option", "key", "value"), &SupersonicGame::set_option);
	ADD_SIGNAL(MethodInfo("session_started", PropertyInfo(Variant::OBJECT, "session")));
	ADD_SIGNAL(MethodInfo("session_ended"));
}

static void add_key(const StringName &action, Key k) {
	Ref<InputEventKey> e;
	e.instantiate();
	e->set_physical_keycode(k);
	InputMap::get_singleton()->action_add_event(action, e);
}

static void add_joy_button(const StringName &action, JoyButton b) {
	Ref<InputEventJoypadButton> e;
	e.instantiate();
	e->set_button_index(b);
	InputMap::get_singleton()->action_add_event(action, e);
}

static void add_joy_axis(const StringName &action, JoyAxis a, real_t dir) {
	Ref<InputEventJoypadMotion> e;
	e.instantiate();
	e->set_axis(a);
	e->set_axis_value(dir);
	InputMap::get_singleton()->action_add_event(action, e);
}

void SupersonicGame::build_input_map() {
	InputMap *im = InputMap::get_singleton();
	const char *actions[] = { "accelerate", "brake", "steer_left", "steer_right", "handbrake", "back", "steer_left_analog", "steer_right_analog", "menu_accept", "menu_up", "menu_down" };
	for (const char *a : actions) {
		if (!im->has_action(a)) {
			im->add_action(a, 0.3);
		}
	}
	add_key("accelerate", KEY_UP);
	add_joy_axis("accelerate", JOY_AXIS_TRIGGER_RIGHT, 1);
	add_joy_button("accelerate", JOY_BUTTON_A);
	add_key("brake", KEY_DOWN);
	add_joy_axis("brake", JOY_AXIS_TRIGGER_LEFT, 1);
	add_joy_button("brake", JOY_BUTTON_X);
	add_key("steer_left", KEY_LEFT);
	add_joy_button("steer_left", JOY_BUTTON_DPAD_LEFT);
	add_key("steer_right", KEY_RIGHT);
	add_joy_button("steer_right", JOY_BUTTON_DPAD_RIGHT);
	add_key("handbrake", KEY_SPACE);
	add_joy_button("handbrake", JOY_BUTTON_B);
	add_joy_button("handbrake", JOY_BUTTON_RIGHT_SHOULDER);
	add_key("back", KEY_ESCAPE);
	add_joy_button("back", JOY_BUTTON_START);
	add_joy_button("back", JOY_BUTTON_BACK);
	add_joy_axis("steer_left_analog", JOY_AXIS_LEFT_X, -1);
	add_joy_axis("steer_right_analog", JOY_AXIS_LEFT_X, 1);
	add_key("menu_accept", KEY_ENTER);
	add_key("menu_accept", KEY_KP_ENTER);
	add_key("menu_accept", KEY_SPACE);
	add_joy_button("menu_accept", JOY_BUTTON_A);
	add_key("menu_up", KEY_UP);
	add_joy_button("menu_up", JOY_BUTTON_DPAD_UP);
	add_key("menu_down", KEY_DOWN);
	add_joy_button("menu_down", JOY_BUTTON_DPAD_DOWN);
}

void SupersonicGame::_ready() {
	set_process_mode(PROCESS_MODE_ALWAYS);
	prefs.load();
	load_progress();
	build_input_map();
	ctrl_unlock_all = Input::get_singleton()->is_key_pressed(KEY_CTRL);

	sound = memnew(SoundSystem);
	sound->set_name("sound");
	add_child(sound);

	ui_layer = memnew(CanvasLayer);
	ui_layer->set_layer(20);
	add_child(ui_layer);
	backdrop = memnew(ColorRect);
	backdrop->set_color(Color(0, 0, 0));
	backdrop->set_anchors_preset(Control::PRESET_FULL_RECT);
	backdrop->set_mouse_filter(Control::MOUSE_FILTER_IGNORE);
	ui_layer->add_child(backdrop);
	stage = memnew(Control);
	stage->set_size(Vector2(640, 480));
	stage->set_clip_contents(true);
	stage->set_mouse_filter(Control::MOUSE_FILTER_IGNORE);
	ui_layer->add_child(stage);
	fader = memnew(ColorRect);
	fader->set_color(Color(0, 0, 0, 0));
	fader->set_anchors_preset(Control::PRESET_FULL_RECT);
	fader->set_mouse_filter(Control::MOUSE_FILTER_IGNORE);
	ui_layer->add_child(fader);
	fps_label = memnew(Label);
	fps_label->set_position(Vector2(6, 460));
	fps_label->add_theme_color_override("font_color", Color(1, 1, 0));
	fps_label->add_theme_color_override("font_outline_color", Color(0, 0, 0));
	fps_label->add_theme_constant_override("outline_size", 4);
	ui_layer->add_child(fps_label);

	Assets::scan_mods();
	load_mods();
	build_options();
	apply_settings();
	build_native_menu();

	if (Assets::has_converted_data()) {
		go_title();
	} else {
		try_auto_import();
	}
}

void SupersonicGame::load_progress() {
	Ref<ConfigFile> cf;
	cf.instantiate();
	if (cf->load("user://progress.cfg") != OK) {
		return;
	}
	for (int i = 1; i < int(ModeId::COUNT); i++) {
		unlocked[i] = bool(cf->get_value("unlocked", mode_script_name(ModeId(i)), false));
	}
}

void SupersonicGame::save_progress() const {
	Ref<ConfigFile> cf;
	cf.instantiate();
	for (int i = 0; i < int(ModeId::COUNT); i++) {
		cf->set_value("unlocked", mode_script_name(ModeId(i)), unlocked[i]);
	}
	cf->save("user://progress.cfg");
}

void SupersonicGame::unlock_mode(ModeId id) {
	unlocked[int(id)] = true;
	save_progress();
}

bool SupersonicGame::is_unlocked(ModeId id) const {
	return unlocked[int(id)] || ctrl_unlock_all;
}

Dictionary SupersonicGame::get_tuning() const {
	return session ? session->tuning().to_dict() : Tuning().to_dict();
}

// ============================================================ settings apply

void SupersonicGame::apply_settings() {
	DisplayServer *ds = DisplayServer::get_singleton();
	Window *win = get_window();
	if (prefs.window_mode == 0) {
		if (ds->window_get_mode() != DisplayServer::WINDOW_MODE_WINDOWED) {
			ds->window_set_mode(DisplayServer::WINDOW_MODE_WINDOWED);
		}
		Rect2i usable = ds->screen_get_usable_rect(ds->window_get_current_screen());
		Vector2i want(prefs.window_w, prefs.window_h);
		if (usable.size.x > 0 && (want.x > usable.size.x || want.y > usable.size.y)) {
			real_t k = MIN(real_t(usable.size.x) / want.x, real_t(usable.size.y) / want.y) * 0.95;
			want = Vector2i(int(want.x * k), int(want.y * k));
		}
		if (ds->window_get_size() != want) {
			ds->window_set_size(want);
			ds->window_set_position(usable.position + (usable.size - want) / 2);
		}
	} else {
		ds->window_set_mode(prefs.window_mode == 2 ? DisplayServer::WINDOW_MODE_EXCLUSIVE_FULLSCREEN : DisplayServer::WINDOW_MODE_FULLSCREEN);
	}
	const DisplayServer::VSyncMode vs[] = { DisplayServer::VSYNC_DISABLED, DisplayServer::VSYNC_ENABLED, DisplayServer::VSYNC_ADAPTIVE, DisplayServer::VSYNC_MAILBOX };
	ds->window_set_vsync_mode(vs[CLAMP(prefs.vsync, 0, 3)]);
	Engine::get_singleton()->set_max_fps(prefs.max_fps);

	Viewport *vp = get_viewport();
	bool forward_plus = RenderingServer::get_singleton()->get_current_rendering_method() == "forward_plus";
	vp->set_scaling_3d_scale(prefs.render_scale);
	Viewport::Scaling3DMode modes[] = { Viewport::SCALING_3D_MODE_BILINEAR, Viewport::SCALING_3D_MODE_FSR, Viewport::SCALING_3D_MODE_FSR2,
		Viewport::SCALING_3D_MODE_METALFX_SPATIAL, Viewport::SCALING_3D_MODE_METALFX_TEMPORAL };
	Viewport::Scaling3DMode sm = modes[CLAMP(prefs.upscaler, 0, 4)];
	if (prefs.render_scale > 1.0) {
		sm = Viewport::SCALING_3D_MODE_BILINEAR; // supersampling
	}
	if (OS::get_singleton()->get_name() != "macOS" && (sm == Viewport::SCALING_3D_MODE_METALFX_SPATIAL || sm == Viewport::SCALING_3D_MODE_METALFX_TEMPORAL)) {
		sm = Viewport::SCALING_3D_MODE_FSR;
	}
	vp->set_scaling_3d_mode(forward_plus ? sm : Viewport::SCALING_3D_MODE_BILINEAR);
	int aa = prefs.antialiasing;
	vp->set_msaa_3d(aa == 1 ? Viewport::MSAA_2X : aa == 2 ? Viewport::MSAA_4X : aa == 3 ? Viewport::MSAA_8X : Viewport::MSAA_DISABLED);
	vp->set_screen_space_aa(aa == 4 ? Viewport::SCREEN_SPACE_AA_FXAA : Viewport::SCREEN_SPACE_AA_DISABLED);
	vp->set_use_taa(aa == 5 && forward_plus);
	vp->set_anisotropic_filtering_level(prefs.texture_filter == 2 ? Viewport::ANISOTROPY_16X : Viewport::ANISOTROPY_DISABLED);

	// Original pacing: 60 updates per real second, 0.0375 sim seconds each.
	Tuning base;
	real_t speed = CLAMP(prefs.game_speed, 0.25, 4.0);
	int substeps = CLAMP(prefs.physics_substeps, 1, 8);
	Engine::get_singleton()->set_physics_ticks_per_second(int(Math::round(base.logic_hz * speed * substeps)));
	Engine::get_singleton()->set_time_scale(base.sim_speed * speed);
	Engine::get_singleton()->set_max_physics_steps_per_frame(8 * substeps);

	if (sound) {
		sound->set_bus_volumes(prefs.music_volume, prefs.effects_volume);
	}
	if (fps_label) {
		fps_label->set_visible(prefs.show_fps);
	}
	(void)win;
}

// ============================================================ fades

void SupersonicGame::fade_in(std::function<void()> done) {
	fade_alpha = 1;
	fade_dir = -1;
	fade_done = done;
}

void SupersonicGame::fade_out(std::function<void()> done) {
	if (fade_dir > 0) {
		return;
	}
	fade_dir = 1;
	fade_done = done;
	input_locked = true;
}

void SupersonicGame::_process(double) {
	uint64_t now = Time::get_singleton()->get_ticks_usec();
	real_t dt_ms = last_usec ? (now - last_usec) / 1000.0 : 0;
	last_usec = now;
	dt_ms = MIN(dt_ms, 100.0);
	if (fade_dir != 0) {
		fade_alpha += fade_dir * dt_ms / 1000.0;
		if ((fade_dir > 0 && fade_alpha >= 1) || (fade_dir < 0 && fade_alpha <= 0)) {
			fade_alpha = CLAMP(fade_alpha, 0.0, 1.0);
			real_t was = fade_dir;
			fade_dir = 0;
			input_locked = false;
			auto cb = fade_done;
			fade_done = nullptr;
			if (cb) {
				cb();
			}
			if (was > 0 && fade_dir == 0) {
				fade_alpha = 0; // the fadeout sprite is destroyed
			}
		}
	}
	fader->set_color(Color(0, 0, 0, fade_alpha));
	layout_stage();
	if (prefs.show_fps) {
		fps_label->set_text(String::num(Engine::get_singleton()->get_frames_per_second(), 0) + " fps");
	}
	update_buttons();
}

// ============================================================ stage / screens

void SupersonicGame::layout_stage() {
	Vector2 vs = get_viewport()->get_visible_rect().size;
	stage->set_position(((vs - Vector2(640, 480)) / 2).floor());
	fps_label->set_position(Vector2(6, vs.y - 22));
	if (settings_panel) {
		settings_panel->set_position(((vs - settings_panel->get_size()) / 2).floor());
	}
}

void SupersonicGame::clear_stage() {
	for (int i = stage->get_child_count() - 1; i >= 0; i--) {
		Node *c = stage->get_child(i);
		stage->remove_child(c);
		c->queue_free();
	}
	buttons.clear();
}

TextureRect *SupersonicGame::add_image(const String &member, bool centered_on_reg) {
	TextureRect *r = memnew(TextureRect);
	r->set_texture(Assets::member_texture(member));
	r->set_texture_filter(CanvasItem::TEXTURE_FILTER_LINEAR);
	r->set_mouse_filter(Control::MOUSE_FILTER_IGNORE);
	Vector2 reg = Assets::member_reg(member);
	r->set_position(centered_on_reg ? Vector2(320, 240) - reg : Vector2());
	stage->add_child(r);
	return r;
}

void SupersonicGame::go_title() {
	screen = Screen::TITLE;
	clear_stage();
	backdrop->set_visible(true);
	add_image("Title-v8");
	fade_in();
}

void SupersonicGame::go_select_level() {
	screen = Screen::SELECT;
	clear_stage();
	backdrop->set_visible(true);
	add_image("Background");
	for (int i = 0; i < 5; i++) {
		Button4 b;
		b.base = MODE_BUTTONS[i];
		b.mode = i;
		b.rect = add_image(b.base);
		buttons.push_back(b);
	}
	fade_in();
	sound->play(8, PlayItem{ "SSRMusicSplashScreen", 0 });
}

void SupersonicGame::go_help(int mode) {
	screen = Screen::HELP;
	pending_mode = mode;
	clear_stage();
	add_image("Help-v5");
	fade_in();
}

void SupersonicGame::update_buttons() {
	if (screen != Screen::SELECT) {
		return;
	}
	Vector2 m = stage->get_local_mouse_position();
	for (Button4 &b : buttons) {
		bool enabled = is_unlocked(ModeId(b.mode));
		String state = enabled ? (b.pressed && b.hover ? "Down" : b.hover ? "Over" : "") : "Gray";
		String member = b.base + state;
		Ref<Texture2D> tex = Assets::member_texture(member);
		if (tex.is_null()) {
			member = b.base;
			tex = Assets::member_texture(member);
		}
		if (b.rect->get_texture() != tex) {
			b.rect->set_texture(tex);
			b.rect->set_size(tex.is_valid() ? tex->get_size() : Vector2());
		}
		b.rect->set_position(Vector2(320, 240) - Assets::member_reg(member));
		Rect2 r(Vector2(320, 240) - Assets::member_reg(b.base), Assets::member_texture(b.base).is_valid() ? Assets::member_texture(b.base)->get_size() : Vector2());
		b.hover = enabled && !input_locked && r.has_point(m);
	}
}

void SupersonicGame::start_mode(int mode) {
	screen = Screen::LOADING;
	clear_stage();
	backdrop->set_visible(true);
	Label *l = memnew(Label);
	l->set_text("Initializing");
	l->add_theme_font_size_override("font_size", 36);
	l->add_theme_color_override("font_color", Color(1, 1, 1));
	l->set_position(Vector2(173, 206) - Vector2(0, 0));
	stage->add_child(l);
	fade_in([this, mode]() {
		// gameTypeManager.realStart
		if (session) {
			session->queue_free();
		}
		session = memnew(DrivingSession);
		add_child(session);
		move_child(session, 0);
		session->setup(this, ModeId(mode));
		session->first_frame();
		emit_signal("session_started", session);
		fade_out([this]() {
			// realStart2
			screen = Screen::DRIVING;
			clear_stage();
			backdrop->set_visible(false);
		});
	});
}

void SupersonicGame::session_finished() {
	if (session) {
		session->queue_free();
		session = nullptr;
	}
	sound->stop_all();
	emit_signal("session_ended");
	go_select_level();
}

void SupersonicGame::_unhandled_input(const Ref<InputEvent> &event) {
	Ref<InputEventKey> key = event;
	if (key.is_valid() && key->is_pressed() && !key->is_echo()) {
		if (key->get_keycode() == KEY_F1 || (key->get_keycode() == KEY_COMMA && key->is_command_or_control_pressed())) {
			toggle_settings_panel();
			return;
		}
		if (key->get_keycode() == KEY_S && !key->is_command_or_control_pressed()) {
			// gameTypeManager: keyPressed("s") toggled sound on/off.
			muted = !muted;
			AudioServer::get_singleton()->set_bus_mute(0, muted);
			return;
		}
		if (key->get_keycode() == KEY_F11 || (key->get_keycode() == KEY_F && key->is_command_or_control_pressed() && key->is_ctrl_pressed())) {
			set_option("window_mode", prefs.window_mode == 0 ? 1 : 0);
			return;
		}
	}
	if (settings_panel && settings_panel->is_visible()) {
		if (event->is_action_pressed("back")) {
			toggle_settings_panel();
		}
		return;
	}
	if (input_locked || fade_dir != 0) {
		return;
	}
	Ref<InputEventMouseButton> mb = event;
	bool click = mb.is_valid() && mb->get_button_index() == MOUSE_BUTTON_LEFT && !mb->is_pressed();
	bool press = mb.is_valid() && mb->get_button_index() == MOUSE_BUTTON_LEFT && mb->is_pressed();
	bool accept = event->is_action_pressed("menu_accept");
	switch (screen) {
		case Screen::TITLE:
			if (click || accept) {
				fade_out([this]() { go_select_level(); });
			} else if (event->is_action_pressed("back")) {
				toggle_settings_panel();
			}
			break;
		case Screen::SELECT: {
			if (press) {
				for (Button4 &b : buttons) {
					b.pressed = b.hover;
				}
			}
			int chosen = -1;
			if (click) {
				for (Button4 &b : buttons) {
					if (b.pressed && b.hover) {
						chosen = b.mode;
					}
					b.pressed = false;
				}
			}
			if (accept) {
				// Keyboard/gamepad: highest unlocked mode, or the hovered one.
				for (int i = 4; i >= 0; i--) {
					if (is_unlocked(ModeId(i))) {
						chosen = i;
						break;
					}
				}
				for (Button4 &b : buttons) {
					if (b.hover) {
						chosen = b.mode;
					}
				}
			}
			if (event->is_action_pressed("back")) {
				fade_out([this]() { go_title(); });
			}
			if (chosen >= 0 && is_unlocked(ModeId(chosen))) {
				sound->play(5, PlayItem{ "buttonclick" });
				fade_out([this, chosen]() { go_help(chosen); });
			}
			break;
		}
		case Screen::HELP:
			if (click || accept) {
				sound->fade_out(8, 500);
				int m = pending_mode;
				fade_out([this, m]() { start_mode(m); });
			} else if (event->is_action_pressed("back")) {
				fade_out([this]() { go_select_level(); });
			}
			break;
		case Screen::IMPORT:
			if (click || accept) {
				DisplayServer::get_singleton()->file_dialog_show("Locate your Supersonic RC files (game.dcr, its folder, or the .zip)",
						OS::get_singleton()->get_system_dir(OS::SYSTEM_DIR_DOWNLOADS), "", false, DisplayServer::FILE_DIALOG_MODE_OPEN_ANY,
						PackedStringArray({ "*.dcr, *.zip ; Supersonic RC game files" }), callable_mp(this, &SupersonicGame::on_file_dialog));
			}
			break;
		default:
			break;
	}
}

void SupersonicGame::_notification(int what) {
	if (what == NOTIFICATION_WM_CLOSE_REQUEST) {
		prefs.save();
	}
}

// ============================================================ import

String SupersonicGame::importer_path() const {
	String env = OS::get_singleton()->get_environment("SSRC_IMPORTER");
	if (!env.is_empty()) {
		return env;
	}
	String exe_dir = OS::get_singleton()->get_executable_path().get_base_dir();
	String exe = OS::get_singleton()->get_name() == "Windows" ? "ssrc-import.exe" : "ssrc-import";
	String p = exe_dir.path_join(exe);
	if (FileAccess::file_exists(p)) {
		return p;
	}
	return ProjectSettings::get_singleton()->globalize_path("res://").path_join("../importer/target/release").path_join(exe);
}

void SupersonicGame::show_import(const String &message) {
	screen = Screen::IMPORT;
	clear_stage();
	backdrop->set_visible(true);
	fade_alpha = 0;
	Label *l = memnew(Label);
	l->set_text(message);
	l->set_autowrap_mode(TextServer::AUTOWRAP_WORD_SMART);
	l->set_horizontal_alignment(HORIZONTAL_ALIGNMENT_CENTER);
	l->set_vertical_alignment(VERTICAL_ALIGNMENT_CENTER);
	l->set_size(Vector2(560, 400));
	l->set_position(Vector2(40, 40));
	l->add_theme_font_size_override("font_size", 18);
	stage->add_child(l);
}

static String find_game_in(const String &dir, int depth) {
	if (dir.is_empty() || !DirAccess::dir_exists_absolute(dir)) {
		return "";
	}
	if (FileAccess::file_exists(dir.path_join("game.dcr")) && FileAccess::file_exists(dir.path_join("GUILibrary.cct"))) {
		return dir.path_join("game.dcr");
	}
	for (const String &f : DirAccess::get_files_at(dir)) {
		String lf = f.to_lower();
		if (lf.ends_with(".zip") && lf.contains("supersonic")) {
			return dir.path_join(f);
		}
	}
	if (depth > 0) {
		for (const String &d : DirAccess::get_directories_at(dir)) {
			if (d.to_lower().contains("supersonic")) {
				String r = find_game_in(dir.path_join(d), depth - 1);
				if (!r.is_empty()) {
					return r;
				}
			}
		}
	}
	return "";
}

void SupersonicGame::try_auto_import() {
	OS *os = OS::get_singleton();
	PackedStringArray dirs;
	String exe_dir = os->get_executable_path().get_base_dir();
	dirs.push_back(exe_dir);
	// Next to the .app bundle (…/Supersonic RC.app/Contents/MacOS -> …).
	dirs.push_back(exe_dir.get_base_dir().get_base_dir().get_base_dir());
	dirs.push_back(os->get_system_dir(OS::SYSTEM_DIR_DOWNLOADS));
	dirs.push_back(os->get_system_dir(OS::SYSTEM_DIR_DESKTOP));
	dirs.push_back(os->get_system_dir(OS::SYSTEM_DIR_DOCUMENTS));
	String home = os->get_environment("HOME");
	if (!home.is_empty()) {
		for (const char *sub : { "Downloads", "Desktop", "Documents", "" }) {
			dirs.push_back(home.path_join(sub));
		}
	}
	for (const String &d : dirs) {
		String found = find_game_in(d, 1);
		UtilityFunctions::print("[ssrc] looking for game files in ", d, found.is_empty() ? String(" - none") : " - found " + found);
		if (!found.is_empty()) {
			show_import("Found your Supersonic RC files:\n" + found + "\n\nConverting them for this Mac. This takes a second...");
			call_deferred("import_from", found);
			return;
		}
	}
	show_import("Welcome to Supersonic RC.\n\nThis port needs your own copy of the original game (game.dcr and GUILibrary.cct, "
				"or the zip that contains them).\n\nClick anywhere to locate the files. They are converted once into open formats "
				"and the original files are never modified.");
}

void SupersonicGame::on_file_dialog(bool status, const PackedStringArray &selected, int) {
	if (status && selected.size() > 0) {
		show_import("Converting your game files...");
		call_deferred("import_from", selected[0]);
	}
}

void SupersonicGame::import_from(const String &path) {
	String dcr = path;
	if (path.to_lower().ends_with(".zip")) {
		Ref<ZIPReader> zip;
		zip.instantiate();
		if (zip->open(path) != OK) {
			show_import("Could not open the zip file:\n" + path + "\n\nClick to try another file.");
			return;
		}
		String tmp = Assets::user_root().path_join("import_tmp");
		DirAccess::make_dir_recursive_absolute(tmp);
		for (const String &f : zip->get_files()) {
			String name = f.get_file();
			String lname = name.to_lower();
			if (lname == "game.dcr" || lname.ends_with(".cct")) {
				Ref<FileAccess> fa = FileAccess::open(tmp.path_join(name), FileAccess::WRITE);
				fa->store_buffer(zip->read_file(f));
				fa->close();
			}
		}
		zip->close();
		dcr = tmp.path_join("game.dcr");
	} else if (DirAccess::dir_exists_absolute(path)) {
		dcr = find_game_in(path, 2);
	}
	if (dcr.is_empty() || !FileAccess::file_exists(dcr)) {
		show_import("game.dcr was not found there.\n\nClick to choose the folder or zip that contains game.dcr and GUILibrary.cct.");
		return;
	}
	String out = Assets::data_root();
	Array output;
	int code = OS::get_singleton()->execute(importer_path(), PackedStringArray({ dcr, out }), output, true);
	if (code != 0 || !Assets::has_converted_data()) {
		String log = output.size() > 0 ? String(output[0]).right(600) : String("(no output)");
		show_import("Conversion failed (code " + String::num_int64(code) + ").\n" + log + "\n\nClick to try again.");
		return;
	}
	Assets::clear_cache();
	go_title();
}

// ============================================================ mods

void SupersonicGame::load_mods() {
	for (const String &dir : Assets::active_mods()) {
		// C++ mods: a GDExtension library declared by <mod>/<name>.gdextension.
		for (const String &f : DirAccess::get_files_at(dir)) {
			if (f.ends_with(".gdextension")) {
				GDExtensionManager::get_singleton()->load_extension(dir.path_join(f));
			}
		}
		Dictionary manifest = load_json_file(dir.path_join("mod.json"));
		String node_class = manifest.get("node_class", "");
		Node *mod_node = nullptr;
		if (!node_class.is_empty() && ClassDB::class_exists(node_class)) {
			mod_node = Object::cast_to<Node>(ClassDB::instantiate(node_class));
		}
		// Script mods: <mod>/mod.gd, attached to a Node that receives signals.
		if (!mod_node && FileAccess::file_exists(dir.path_join("mod.gd"))) {
			Ref<Script> s = ResourceLoader::get_singleton()->load(dir.path_join("mod.gd"));
			if (s.is_valid()) {
				mod_node = memnew(Node);
				mod_node->set_script(s);
			}
		}
		if (mod_node) {
			mod_node->set_name(String("mod_") + dir.get_file());
			mod_node->set_meta("game", this);
			add_child(mod_node);
		}
	}
}

void SupersonicGame::reload_mods() {
	Assets::scan_mods();
	UtilityFunctions::print("[ssrc] mods reloaded; changes apply to the next race");
}

// ============================================================ options

void SupersonicGame::build_options() {
	auto pct = [](std::initializer_list<int> v) {
		std::vector<Choice> c;
		for (int x : v) {
			c.push_back({ String::num_int64(x) + "%", x / 100.0 });
		}
		return c;
	};
	options = {
		{ "Display", "Window", "window_mode", { { "Windowed", 0 }, { "Fullscreen", 1 }, { "Exclusive Fullscreen", 2 } } },
		{ "Display", "Window Size", "window_size", { { "960 x 720", 0 }, { "1280 x 960", 1 }, { "1440 x 1080", 2 }, { "1600 x 1200", 3 }, { "1280 x 720", 4 }, { "1920 x 1080", 5 }, { "2560 x 1440", 6 }, { "3840 x 2160 (4K)", 7 } } },
		{ "Display", "Render Resolution", "render_scale", pct({ 50, 67, 75, 100, 125, 150, 200 }) },
		{ "Display", "Upscaler", "upscaler", { { "Bilinear", 0 }, { "AMD FSR 1", 1 }, { "AMD FSR 2", 2 }, { "MetalFX Spatial", 3 }, { "MetalFX Temporal", 4 } } },
		{ "Display", "Anti-aliasing", "antialiasing", { { "Off", 0 }, { "MSAA 2x", 1 }, { "MSAA 4x", 2 }, { "MSAA 8x", 3 }, { "FXAA", 4 }, { "TAA", 5 } } },
		{ "Display", "V-Sync", "vsync", { { "Off", 0 }, { "On", 1 }, { "Adaptive", 2 }, { "Mailbox", 3 } } },
		{ "Display", "Frame Rate Limit", "max_fps", { { "30", 30 }, { "60", 60 }, { "120", 120 }, { "144", 144 }, { "240", 240 }, { "Unlimited", 0 } } },
		{ "Display", "Show FPS", "show_fps", { { "Off", 0 }, { "On", 1 } } },
		{ "Graphics", "Lighting", "lighting", { { "Original", 0 }, { "Enhanced (shadows, SSAO)", 1 } } },
		{ "Graphics", "Texture Filtering", "texture_filter", { { "Nearest (retro)", 0 }, { "Linear", 1 }, { "Anisotropic 16x", 2 } } },
		{ "Graphics", "Field of View", "fov", { { "60", 60 }, { "70 (original)", 0 }, { "80", 80 }, { "90", 90 }, { "100", 100 } } },
		{ "Graphics", "Camera", "camera_mode", { { "Chase (original)", 0 }, { "Overhead", 1 } } },
		{ "Gameplay", "Game Speed", "game_speed", pct({ 50, 75, 100, 125, 150, 200 }) },
		{ "Gameplay", "Physics Rate", "physics_substeps", { { "60 Hz (original)", 1 }, { "120 Hz", 2 }, { "240 Hz", 4 }, { "480 Hz", 8 } } },
		{ "Audio", "Music Volume", "music_volume", pct({ 0, 25, 50, 75, 100 }) },
		{ "Audio", "Effects Volume", "effects_volume", pct({ 0, 25, 50, 75, 100 }) },
	};
}

static const Vector2i WINDOW_SIZES[] = { { 960, 720 }, { 1280, 960 }, { 1440, 1080 }, { 1600, 1200 }, { 1280, 720 }, { 1920, 1080 }, { 2560, 1440 }, { 3840, 2160 } };

double SupersonicGame::get_option(const String &key) const {
	if (key == "window_mode") return prefs.window_mode;
	if (key == "window_size") {
		for (int i = 0; i < 8; i++) {
			if (WINDOW_SIZES[i] == Vector2i(prefs.window_w, prefs.window_h)) return i;
		}
		return -1;
	}
	if (key == "render_scale") return prefs.render_scale;
	if (key == "upscaler") return prefs.upscaler;
	if (key == "antialiasing") return prefs.antialiasing;
	if (key == "vsync") return prefs.vsync;
	if (key == "max_fps") return prefs.max_fps;
	if (key == "show_fps") return prefs.show_fps ? 1 : 0;
	if (key == "lighting") return prefs.lighting;
	if (key == "texture_filter") return prefs.texture_filter;
	if (key == "fov") return prefs.fov;
	if (key == "camera_mode") return prefs.camera_mode;
	if (key == "game_speed") return prefs.game_speed;
	if (key == "physics_substeps") return prefs.physics_substeps;
	if (key == "music_volume") return prefs.music_volume;
	if (key == "effects_volume") return prefs.effects_volume;
	return 0;
}

void SupersonicGame::set_option(const String &key, double v) {
	if (key == "window_mode") prefs.window_mode = int(v);
	else if (key == "window_size") {
		int i = CLAMP(int(v), 0, 7);
		prefs.window_w = WINDOW_SIZES[i].x;
		prefs.window_h = WINDOW_SIZES[i].y;
		prefs.window_mode = 0;
	} else if (key == "render_scale") prefs.render_scale = v;
	else if (key == "upscaler") prefs.upscaler = int(v);
	else if (key == "antialiasing") prefs.antialiasing = int(v);
	else if (key == "vsync") prefs.vsync = int(v);
	else if (key == "max_fps") prefs.max_fps = int(v);
	else if (key == "show_fps") prefs.show_fps = v > 0.5;
	else if (key == "lighting") prefs.lighting = int(v);
	else if (key == "texture_filter") prefs.texture_filter = int(v);
	else if (key == "fov") prefs.fov = v;
	else if (key == "camera_mode") prefs.camera_mode = int(v);
	else if (key == "game_speed") prefs.game_speed = v;
	else if (key == "physics_substeps") prefs.physics_substeps = int(v);
	else if (key == "music_volume") prefs.music_volume = v;
	else if (key == "effects_volume") prefs.effects_volume = v;
	prefs.save();
	apply_settings();
	refresh_native_menu();
	if (session && (key == "fov")) {
		ChaseCamera *c = session->get_camera();
		if (c && c->camera) {
			c->camera->set_fov(prefs.fov > 0 ? prefs.fov : session->tuning().camera_fov);
		}
	}
}

void SupersonicGame::run_action(const String &action) {
	if (action == "main_menu") {
		if (session) {
			session->request_exit();
		} else {
			fade_out([this]() { go_title(); });
		}
	} else if (action == "unlock_all") {
		for (int i = 0; i < int(ModeId::COUNT); i++) {
			unlocked[i] = true;
		}
		save_progress();
	} else if (action == "reset_progress") {
		for (int i = 1; i < int(ModeId::COUNT); i++) {
			unlocked[i] = false;
		}
		save_progress();
	} else if (action == "open_mods") {
		DirAccess::make_dir_recursive_absolute(Assets::mods_root());
		OS::get_singleton()->shell_show_in_file_manager(Assets::mods_root());
	} else if (action == "reload_mods") {
		reload_mods();
	} else if (action == "open_data") {
		OS::get_singleton()->shell_show_in_file_manager(Assets::data_root());
	} else if (action == "reimport") {
		DisplayServer::get_singleton()->file_dialog_show("Locate your Supersonic RC files", OS::get_singleton()->get_system_dir(OS::SYSTEM_DIR_DOWNLOADS), "", false,
				DisplayServer::FILE_DIALOG_MODE_OPEN_ANY, PackedStringArray({ "*.dcr, *.zip ; Supersonic RC game files" }), callable_mp(this, &SupersonicGame::on_file_dialog));
	} else if (action == "settings") {
		toggle_settings_panel();
	} else if (action == "quit") {
		prefs.save();
		get_tree()->quit();
	}
}

// ---- macOS menu bar (NativeMenu); other platforms use the in-game panel ----

void SupersonicGame::build_native_menu() {
	NativeMenu *nm = NativeMenu::get_singleton();
	if (!nm || !nm->has_feature(NativeMenu::FEATURE_GLOBAL_MENU)) {
		return;
	}
	RID main = nm->get_system_menu(NativeMenu::MAIN_MENU_ID);
	RID app = nm->get_system_menu(NativeMenu::APPLICATION_MENU_ID);
	Callable cb(this, "on_native_menu");
	nm->add_item(app, "Settings...", cb, Callable(), Array({ "action", "settings" }), Key(KEY_MASK_META | KEY_COMMA), 0);

	auto action = [&](RID menu, const String &label, const String &id, Key accel = KEY_NONE) {
		nm->add_item(menu, label, cb, Callable(), Array({ "action", id }), accel);
	};
	RID game_menu = nm->create_menu();
	action(game_menu, "Main Menu", "main_menu");
	nm->add_separator(game_menu);
	action(game_menu, "Unlock All Modes", "unlock_all");
	action(game_menu, "Reset Progress", "reset_progress");
	nm->add_separator(game_menu);
	action(game_menu, "Open Mods Folder", "open_mods");
	action(game_menu, "Reload Mods", "reload_mods");
	nm->add_separator(game_menu);
	action(game_menu, "Re-import Game Files...", "reimport");
	action(game_menu, "Show Converted Data", "open_data");
	nm->add_submenu_item(main, "Game", game_menu);

	const char *sections[] = { "Display", "Graphics", "Gameplay", "Audio" };
	menu_items.clear();
	for (const char *sec : sections) {
		RID sm = nm->create_menu();
		for (int oi = 0; oi < int(options.size()); oi++) {
			const Option &o = options[oi];
			if (o.section != sec) {
				continue;
			}
			RID sub = nm->create_menu();
			for (const Choice &c : o.choices) {
				int idx = nm->add_radio_check_item(sub, c.label, cb, Callable(), Array({ "option", oi, c.value }));
				menu_items.push_back({ sub, oi });
				nm->set_item_tag(sub, idx, Array({ "option", oi, c.value }));
			}
			nm->add_submenu_item(sm, o.label, sub);
		}
		nm->add_submenu_item(main, sec, sm);
	}
	refresh_native_menu();
}

void SupersonicGame::refresh_native_menu() {
	NativeMenu *nm = NativeMenu::get_singleton();
	if (!nm || !nm->has_feature(NativeMenu::FEATURE_GLOBAL_MENU)) {
		return;
	}
	std::vector<RID> seen;
	for (auto &mi : menu_items) {
		RID sub = mi.first;
		bool done = false;
		for (const RID &r : seen) {
			done = done || r == sub;
		}
		if (done) {
			continue;
		}
		seen.push_back(sub);
		for (int i = 0; i < nm->get_item_count(sub); i++) {
			Array tag = nm->get_item_tag(sub, i);
			if (tag.size() == 3) {
				const Option &o = options[int(tag[1])];
				nm->set_item_checked(sub, i, Math::is_equal_approx(double(tag[2]), get_option(o.key)));
			}
		}
	}
}

void SupersonicGame::on_native_menu(const Variant &tag) {
	Array t = tag;
	if (t.size() >= 2 && String(t[0]) == "action") {
		run_action(t[1]);
	} else if (t.size() == 3 && String(t[0]) == "option") {
		set_option(options[int(t[1])].key, t[2]);
	}
}

void SupersonicGame::on_option_selected(int idx, Object *game, const String &key, const Array &values) {
	if (SupersonicGame *g = Object::cast_to<SupersonicGame>(game)) {
		g->set_option(key, values[idx]);
	}
}

// ---- in-game settings panel (F1 / Cmd+, / Esc on the title screen) ----

void SupersonicGame::build_settings_panel() {
	settings_panel = memnew(PanelContainer);
	settings_panel->set_process_mode(PROCESS_MODE_ALWAYS);
	Ref<StyleBoxFlat> sb;
	sb.instantiate();
	sb->set_bg_color(Color(0.08, 0.09, 0.12, 0.94));
	sb->set_corner_radius_all(10);
	sb->set_content_margin_all(16);
	settings_panel->add_theme_stylebox_override("panel", sb);
	ScrollContainer *scroll = memnew(ScrollContainer);
	scroll->set_custom_minimum_size(Vector2(560, 420));
	settings_panel->add_child(scroll);
	VBoxContainer *vb = memnew(VBoxContainer);
	vb->set_h_size_flags(Control::SIZE_EXPAND_FILL);
	scroll->add_child(vb);
	Label *title = memnew(Label);
	title->set_text("Settings  (F1 or Esc to close)");
	title->add_theme_font_size_override("font_size", 18);
	vb->add_child(title);
	String last_section;
	for (int oi = 0; oi < int(options.size()); oi++) {
		const Option &o = options[oi];
		if (o.section != last_section) {
			Label *s = memnew(Label);
			s->set_text(o.section);
			s->add_theme_color_override("font_color", Color(1, 0.8, 0.2));
			vb->add_child(s);
			last_section = o.section;
		}
		HBoxContainer *row = memnew(HBoxContainer);
		Label *l = memnew(Label);
		l->set_text(o.label);
		l->set_custom_minimum_size(Vector2(200, 0));
		row->add_child(l);
		OptionButton *ob = memnew(OptionButton);
		ob->set_h_size_flags(Control::SIZE_EXPAND_FILL);
		double cur = get_option(o.key);
		for (int ci = 0; ci < int(o.choices.size()); ci++) {
			ob->add_item(o.choices[ci].label, ci);
			if (Math::is_equal_approx(o.choices[ci].value, cur)) {
				ob->select(ci);
			}
		}
		Array values;
		for (const Choice &c : o.choices) {
			values.push_back(c.value);
		}
		ob->connect("item_selected", callable_mp_static(&SupersonicGame::on_option_selected).bind(this, o.key, values));
		row->add_child(ob);
		vb->add_child(row);
	}
	HBoxContainer *actions = memnew(HBoxContainer);
	const char *acts[][2] = { { "Main Menu", "main_menu" }, { "Unlock All", "unlock_all" }, { "Mods Folder", "open_mods" }, { "Quit", "quit" } };
	for (auto &a : acts) {
		Button *b = memnew(Button);
		b->set_text(a[0]);
		b->connect("pressed", Callable(this, "run_action").bind(String(a[1])));
		actions->add_child(b);
	}
	vb->add_child(actions);
	ui_layer->add_child(settings_panel);
	settings_panel->set_visible(false);
}

void SupersonicGame::toggle_settings_panel() {
	if (!settings_panel) {
		build_settings_panel();
	}
	bool show = !settings_panel->is_visible();
	if (show) {
		// Rebuild to reflect current values.
		settings_panel->queue_free();
		settings_panel = nullptr;
		build_settings_panel();
	}
	settings_panel->set_visible(show);
	settings_panel->reset_size();
	layout_stage();
	get_tree()->set_pause(show && screen == Screen::DRIVING);
}

} // namespace ssrc
