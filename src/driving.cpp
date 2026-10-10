#include "driving.h"
#include "assets.h"
#include "game.h"
#include "materials.h"
#include "sound.h"

#include <godot_cpp/classes/box_shape3d.hpp>
#include <godot_cpp/classes/collision_shape3d.hpp>
#include <godot_cpp/classes/concave_polygon_shape3d.hpp>
#include <godot_cpp/classes/directional_light3d.hpp>
#include <godot_cpp/classes/environment.hpp>
#include <godot_cpp/classes/font_variation.hpp>
#include <godot_cpp/classes/geometry3d.hpp>
#include <godot_cpp/classes/input.hpp>
#include <godot_cpp/classes/resource_loader.hpp>
#include <godot_cpp/classes/os.hpp>
#include <godot_cpp/classes/mesh.hpp>
#include <godot_cpp/classes/physics_direct_space_state3d.hpp>
#include <godot_cpp/classes/physics_material.hpp>
#include <godot_cpp/classes/physics_ray_query_parameters3d.hpp>
#include <godot_cpp/classes/physics_shape_query_parameters3d.hpp>
#include <godot_cpp/classes/scene_tree.hpp>
#include <godot_cpp/classes/standard_material3d.hpp>
#include <godot_cpp/classes/static_body3d.hpp>
#include <godot_cpp/classes/theme_db.hpp>
#include <godot_cpp/classes/time.hpp>
#include <godot_cpp/classes/viewport.hpp>
#include <godot_cpp/classes/world3d.hpp>
#include <godot_cpp/classes/world_environment.hpp>
#include <godot_cpp/variant/utility_functions.hpp>

namespace ssrc {

const char *mode_script_name(ModeId id) {
	switch (id) {
		case ModeId::FREE: return "free_driving";
		case ModeId::CHECKPOINT: return "checkpoint_chase";
		case ModeId::STUNT: return "stunt_rally";
		case ModeId::SCAVENGER: return "scavenger_hunt";
		case ModeId::SUPERSONIC: return "supersonic_challenge";
		default: return "";
	}
}

Transform3D director_to_godot_xform(const Transform3D &d) {
	Transform3D c(axis_map().scaled(Vector3(WORLD_SCALE, WORLD_SCALE, WORLD_SCALE)), Vector3());
	return c * d;
}

static String sanitize(const String &s) {
	String out;
	for (int i = 0; i < s.length(); i++) {
		char32_t c = s[i];
		bool ok = (c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') || (c >= '0' && c <= '9') || c == '-' || c == '_';
		out += ok ? String::chr(c) : String("_");
	}
	return out;
}

static Color rgb_hex(const String &hex) {
	return Color::html(hex);
}

static String pad(int v, int len, const String &with, bool right = false) {
	String s = String::num_int64(v);
	while (s.length() < len) {
		s = right ? s + with : with + s;
	}
	return s;
}

// The movie script's getStuntText().
static String stunt_text(int points) {
	int p = points / 10;
	static const struct {
		int over;
		const char *text;
	} table[] = {
		{ 80, "Inconceivable!" }, { 75, "Phenomenal!" }, { 70, "Huge stunt!" }, { 65, "Fantastic stunt!" },
		{ 60, "Mind-blowing!" }, { 55, "Extraordinary!" }, { 50, "Astonishing!" }, { 45, "Unbelievable!" },
		{ 40, "Wicked stunt!" }, { 35, "Incredible!" }, { 30, "Awesome!" }, { 25, "Wow!" },
		{ 20, "Amazing!" }, { 15, "Great stunt!" }, { 10, "Nice one!" }, { 5, "Nice stunt!" },
	};
	for (const auto &e : table) {
		if (p > e.over) {
			return e.text;
		}
	}
	return "Not bad!";
}

// ============================================================ HUD

// The original rendered HUD text with Verdana (bold italic captions,
// italic digits). DejaVu Sans shares Verdana's wide humanist design and is
// freely redistributable; italics are synthesised the way Director did.
static Ref<Font> make_hud_font(bool bold) {
	Ref<Font> base = ResourceLoader::get_singleton()->load(bold ? "res://fonts/DejaVuSans-Bold.ttf" : "res://fonts/DejaVuSans.ttf");
	if (base.is_null()) {
		base = ThemeDB::get_singleton()->get_fallback_font();
	}
	Ref<FontVariation> it;
	it.instantiate();
	it->set_base_font(base);
	it->set_variation_transform(Transform2D(Vector2(1, 0), Vector2(0.2, 1), Vector2()));
	return it;
}

Hud::Hud(Node *parent) {
	font_regular = make_hud_font(false);
	font_bold_italic = make_hud_font(true);
	layer = memnew(CanvasLayer);
	layer->set_name("hud");
	layer->set_layer(10);
	parent->add_child(layer);
	root = memnew(Control);
	root->set_anchors_preset(Control::PRESET_FULL_RECT);
	root->set_mouse_filter(Control::MOUSE_FILTER_IGNORE);
	layer->add_child(root);

	timer_caption = make_label(12, true, Color(1, 1, 1));
	timer_caption->set_text("Time remaining");
	for (int i = 0; i < 5; i++) {
		timer_digits.push_back(make_label(21, true, Color(1, 1, 1)));
	}
	message = memnew(VBoxContainer);
	message->add_theme_constant_override("separation", 0);
	message->set_alignment(BoxContainer::ALIGNMENT_BEGIN);
	message->set_mouse_filter(Control::MOUSE_FILTER_IGNORE);
	root->add_child(message);
	message->set_modulate(Color(1, 1, 1, 0));
}

Hud::~Hud() {
	// The layer is a child of the session and is freed with it.
}

Label *Hud::make_label(int size, bool bold_italic, const Color &c) {
	Label *l = memnew(Label);
	l->add_theme_font_override("font", bold_italic ? font_bold_italic : font_regular);
	l->add_theme_font_size_override("font_size", size);
	l->add_theme_color_override("font_color", c);
	l->set_horizontal_alignment(HORIZONTAL_ALIGNMENT_CENTER);
	l->set_mouse_filter(Control::MOUSE_FILTER_IGNORE);
	root->add_child(l);
	return l;
}

void Hud::setup_points(bool with_goal) {
	points_goal = with_goal;
	points_caption = make_label(12, true, Color(1, 1, 1));
	int n = with_goal ? 11 : 5;
	for (int i = 0; i < n; i++) {
		Color c(1, 1, 1);
		if (with_goal && i < 5) {
			c = Color(1, 0, 0);
		} else if (with_goal && i > 5) {
			c = Color(0, 1, 0);
		}
		points_digits.push_back(make_label(21, true, c));
	}
	if (with_goal) {
		points_caption->set_text(String::utf8("Score / Goal"));
		// word[1] red, word[3] green: use a RichText-free approximation.
		points_caption->add_theme_color_override("font_color", Color(1, 1, 1));
	} else {
		points_caption->set_text("Points");
	}
	layout();
}

void Hud::place_digits(std::vector<Label *> &digits, int count, real_t x0, real_t anchor) {
	for (int i = 0; i < count && i < int(digits.size()); i++) {
		real_t cx = x0 + (i + 1) * 18 + anchor; // insceneoverlay centre
		Label *l = digits[i];
		l->set_size(Vector2(18, 30));
		l->set_position(Vector2(cx - 8 - 9, 50 - 32));
	}
}

void Hud::layout() {
	Vector2 vs = root->get_viewport_rect().size;
	real_t right = vs.x - 640.0;
	timer_caption->set_size(Vector2(200, 16));
	timer_caption->set_position(Vector2(72 - 100, 20 - 12));
	place_digits(timer_digits, 5, 16, 0);
	if (points_caption) {
		real_t cx = (points_goal ? 534 : 582) + right;
		points_caption->set_size(Vector2(200, 16));
		points_caption->set_position(Vector2(cx - 100, 20 - 12));
		place_digits(points_digits, int(points_digits.size()), points_goal ? 426 : 526, right);
	}
	Vector2 ms = message->get_combined_minimum_size();
	real_t h = 2;
	while (h < ms.y) {
		h *= 2;
	}
	message->set_size(Vector2(512, ms.y));
	message->set_position(Vector2(320 + right / 2 - 256, 400 + (vs.y - 480) - h / 2));
}

void Hud::set_timer(const String &mmss) {
	for (int i = 0; i < 5; i++) {
		timer_digits[i]->set_text(i < mmss.length() ? mmss.substr(i, 1) : "");
	}
}

void Hud::show_timer(bool on) {
	timer_caption->set_visible(on);
	for (Label *l : timer_digits) {
		l->set_visible(on);
	}
}

void Hud::set_points(const String &text, bool goal_reached) {
	for (int i = 0; i < int(points_digits.size()); i++) {
		points_digits[i]->set_text(i < text.length() ? text.substr(i, 1) : "");
		if (points_goal && i < 5) {
			points_digits[i]->add_theme_color_override("font_color", goal_reached ? Color(0, 1, 0) : Color(1, 0, 0));
		}
	}
}

void Hud::set_message(const String &text) {
	for (int i = message->get_child_count() - 1; i >= 0; i--) {
		Node *c = message->get_child(i);
		message->remove_child(c);
		c->queue_free();
	}
	PackedStringArray lines = text.split("\r");
	// textmessagemanager sizes 24/18/12 pt, as they appear on the 640x480 stage.
	const int sizes[] = { 19, 14, 10 };
	for (int i = 0; i < lines.size(); i++) {
		Label *l = memnew(Label);
		l->add_theme_font_override("font", font_bold_italic);
		l->add_theme_font_size_override("font_size", sizes[MIN(i, 2)]);
		l->add_theme_color_override("font_color", Color(1, 1, 1));
		l->add_theme_color_override("font_shadow_color", Color(0, 0, 0, 0.35));
		l->set_horizontal_alignment(HORIZONTAL_ALIGNMENT_CENTER);
		l->set_text(lines[i]);
		message->add_child(l);
	}
	message_blend = 0;
	message_target = 1;
	message->set_modulate(Color(1, 1, 1, 0));
	layout();
}

void Hud::fade_out_message() {
	message_target = 0;
}

void Hud::update(real_t dt_ms) {
	real_t k = 1.0 - Math::exp(-dt_ms / 450.0);
	message_blend += (message_target - message_blend) * k;
	if (message_target == 0 && message_blend < 0.01) {
		message_blend = 0;
	}
	message->set_modulate(Color(1, 1, 1, message_blend));
}

// ============================================================ Camera

ChaseCamera::ChaseCamera(DrivingSession *s, bool from_a_distance) :
		session(s), distance_mode(from_a_distance) {
	camera = memnew(Camera3D);
	camera->set_name("camera");
	session->add_child(camera);
	const Tuning &t = s->tuning();
	real_t fov = session->get_game()->settings().fov > 0 ? session->get_game()->settings().fov : t.camera_fov;
	camera->set_fov(distance_mode ? 45 : fov);
	camera->set_near(0.05);
	camera->set_far(2000);
	camera->set_current(true);
	position = distance_mode ? Vector3(0, -584, 250) : Vector3(-2000, -584, 250);
	wanna_be = position;
}

void ChaseCamera::update() {
	Car *car = session->get_car();
	const Tuning &t = session->tuning();
	Vector3 car_pos = car->xform.origin;
	if (distance_mode) {
		// camera_fromadistance
		Vector3 p = car_pos;
		p.z = 0;
		Vector3 me = position;
		me.z = 0;
		Vector3 dif = p - me;
		if (dif.length() > 480) {
			me = me + dif.normalized() * 10;
		}
		me.z = MAX(640.0, car_pos.z + 200);
		position = me;
		camera->look_at_from_position(d2g(position), d2g(car_pos), Vector3(0, 1, 0));
		return;
	}
	if (!orbit) {
		bool dont_follow = car->linear_velocity.length() < t.camera_follow_min_speed;
		if (following && !dont_follow) {
			Vector3 behind = -car->linear_velocity;
			behind.z = 0;
			behind.normalize();
			wanna_be = car_pos + behind * t.camera_distance;
		} else {
			Vector3 behind = -car->xform.y_axis();
			wanna_be = car_pos + behind * t.camera_distance;
			if (car_pos.distance_to(position) > 300) {
				following = true;
			}
		}
		wanna_be.z = car_pos.z + t.camera_height;
	} else {
		real_t tt = session->real_ms() / 1000.0;
		wanna_be = car_pos + Vector3(Math::sin(tt) * 150, Math::cos(tt) * 150, 50);
	}
	position = position + (wanna_be - position) * t.camera_lerp;
	Vector3 target = car_pos + Vector3(0, 0, 10);
	Vector3 to_cam = position - target;
	real_t dist = to_cam.length();
	if (dist > 0.001) {
		RayHit hit = session->raycast(target, to_cam / dist, dist, false);
		if (hit.hit && hit.position.distance_to(target) < dist) {
			// Original: camera = hit point. Pull it 3 units toward the car so
			// the near plane does not slice through the wall it touches.
			Vector3 back = (target - hit.position);
			real_t bl = back.length();
			position = hit.position + (bl > 3.0 ? back / bl * 3.0 : back * 0.5);
		}
	}
	Vector3 up = d2g_dir(Vector3(0, 0.01, 1)).normalized();
	if ((d2g(target) - d2g(position)).length() > 1e-4) {
		camera->look_at_from_position(d2g(position), d2g(target), up);
	}
}

// ============================================================ Game modes

GameMode::GameMode(DrivingSession *s, ModeId p_id) :
		id(p_id), session(s) {}

void GameMode::text(const String &t) {
	if (text_stopped) {
		return;
	}
	session->get_hud()->set_message(t);
	text_started_ms = session->real_ms();
}

void GameMode::timer_set(real_t seconds) {
	timer_start = Math::round(session->sim_time() / session->tuning().sim_seconds_per_clock_second);
	timer_total = seconds;
}

void GameMode::timer_add(real_t seconds) {
	timer_total += seconds;
	last_oink = int(MIN(10.0, timer_total));
}

int GameMode::timer_get() {
	if (timer_stopped) {
		return timer_stop_value;
	}
	int t = int(MAX(0.0, Math::round(-session->sim_time() / session->tuning().sim_seconds_per_clock_second + timer_start + timer_total)));
	if (t < last_oink) {
		last_oink = t;
		session->sound()->play(4, PlayItem{ "alarm" });
	}
	return t;
}

void GameMode::timer_stop() {
	timer_stop_value = timer_get();
	timer_stopped = true;
}

void GameMode::refresh_points() {
	if (uses_goal_counter()) {
		session->get_hud()->set_points(pad(points, 5, " ") + "/" + pad(goal, 5, " ", true), goal_reached);
	} else {
		session->get_hud()->set_points(pad(points, 5, "0"), false);
	}
}

void GameMode::add_points(int p) {
	points = MIN(99999, points + p);
	if (uses_goal_counter() && points >= goal && !goal_reached) {
		goal_reached = true;
		reached_goal();
	}
	refresh_points();
}

void GameMode::set_goal(int g) {
	goal = g;
	refresh_points();
}

void GameMode::real_start() {
	timer_set(20);
	session->get_hud()->setup_points(uses_goal_counter());
	refresh_points();
}

void GameMode::stop_driving() {
	stopping = true;
	timer_stop();
	session->get_camera()->car_stopped_driving();
	session->get_car()->stop_driving();
}

void GameMode::finish_after(real_t ms) {
	finish_at_ms = session->real_ms() + ms;
}

void GameMode::stunt_event(int p) {
	text(stunt_text(p) + "\r" + String::num_int64(p) + " Points!");
	add_points(p);
}

void GameMode::havok_update() {}

void GameMode::stunt_update() {
	Car *car = session->get_car();
	bool in_air = true;
	for (Wheel *w : car->wheels) {
		if (w->on_ground) {
			in_air = false;
		}
	}
	if (jump_in_progress) {
		angular_distance += car->angular_velocity.length() * (1.0 / 60.0);
		jump_max_height = MAX(car->xform.origin.z - jump_start_pos.z, jump_max_height);
	}
	if (!in_air && jump_in_progress) {
		real_t duration = Math::round(session->sim_time() - jump_start_time);
		real_t pts = 2;
		pts *= MAX(1.0, duration / 20.0);
		pts *= MAX(1.0, angular_distance);
		pts *= 10;
		if (pts > 20) {
			stunt_event(int(Math::round(pts)));
		}
	}
	if (in_air && !jump_in_progress) {
		jump_start_time = session->sim_time();
		jump_start_pos = car->xform.origin;
		jump_max_height = 0;
		angular_distance = 0;
	}
	jump_in_progress = in_air;
}

void GameMode::pickup_update() {
	Car *car = session->get_car();
	const Tuning &t = session->tuning();
	bool now_upside_down = false;
	if (car->xform.origin.z < -10) {
		Vector3 p = car->xform.origin;
		p.z = 800;
		RayHit hit = session->raycast(p, Vector3(0, 0, -1), 5000, true);
		Vector3 np = hit.hit ? hit.position + Vector3(0, 0, 30) : Vector3(p.x, p.y, 100);
		car->reset_to(np, false);
	}
	if (car->linear_velocity.length() < 10) {
		real_t ang = Math::rad_to_deg(car->xform.z_axis().angle_to(Vector3(0, 0, -1)));
		if (ang < 20) {
			if (!upside_down) {
				upside_down_start = session->sim_time();
			} else if (session->sim_time() - upside_down_start > t.flip_recover_seconds) {
				car->teleport_up(20);
			}
			upside_down = true;
			now_upside_down = true;
		}
	}
	if (!now_upside_down) {
		upside_down = false;
	}
}

void GameMode::step_frame() {
	// timer.stepFrame
	int t = timer_get();
	if (t == 0 && !timer_out) {
		timer_out = true;
		out_of_time();
	}
	t = timer_get();
	session->get_hud()->set_timer(pad(t / 60, 2, "0") + ":" + pad(t - (t / 60) * 60, 2, "0"));
	// textmessagemanager.stepFrame
	if (text_started_ms >= 0 && session->real_ms() > text_started_ms + 5000) {
		session->get_hud()->fade_out_message();
		text_started_ms = -1;
	}
	pickup_update();
	if (finish_at_ms >= 0 && session->real_ms() > finish_at_ms) {
		finish_at_ms = -1;
		session->finish();
	}
}

// ---- checkpoint_gametype ----

void CheckpointMode::real_start() {
	GameMode::real_start();
	checkpoint_pos = session->group_positions("checkpoint_", 1000, true);
	for (const Vector3 &p : checkpoint_pos) {
		checkpoints.push_back(session->clone_prop("scenes/star.glb", p - Vector3(0, 0, 50), 3.0));
	}
	checkpoint_index = 1;
	if (!checkpoint_pos.empty()) {
		session->arrow_point_at(checkpoint_pos[0]);
	}
}

void CheckpointMode::arrow_point_at(int index) {
	if (index >= 1 && index <= int(checkpoint_pos.size())) {
		session->arrow_point_at(checkpoint_pos[index - 1]);
	}
}

void CheckpointMode::hide_checkpoints() {
	for (Node3D *c : checkpoints) {
		c->set_visible(false);
	}
	hidden = true;
}

void CheckpointMode::show_this_checkpoint(int index) {
	for (int i = 0; i < int(checkpoints.size()); i++) {
		checkpoints[i]->set_visible(!hidden && i + 1 == index);
	}
}

void CheckpointMode::havok_update() {
	GameMode::havok_update();
	if (stopping || hidden || checkpoints.empty()) {
		return;
	}
	Vector3 cp = session->prop_position(checkpoints[checkpoint_index - 1]);
	if (session->get_car()->xform.origin.distance_to(cp) < session->tuning().checkpoint_radius) {
		session->sound()->play(4, PlayItem{ "stingcheckpoint" });
		passed_checkpoint();
		show_this_checkpoint(checkpoint_index);
	}
	if (checkpoint_index <= int(checkpoints.size()) && !hidden) {
		session->spin_prop(checkpoints[checkpoint_index - 1], 3);
	}
}

// ---- free_driving ----
class FreeDriving : public CheckpointMode {
public:
	using CheckpointMode::CheckpointMode;
	bool uses_goal_counter() const override { return false; }
	void real_start() override {
		CheckpointMode::real_start();
		timer_set(150);
	}
	void passed_checkpoint() override {
		text("Checkpoint!\r+2 Seconds");
		timer_add(2);
		checkpoint_index++;
		if (checkpoint_index > int(checkpoints.size())) {
			checkpoint_index = 1;
			laps++;
			if (!reached && laps > 1) {
				session->sound()->play(4, PlayItem{ "stingwin" });
				session->unlock(ModeId::CHECKPOINT);
				text("You did it!\rCheckpoint Chase unlocked!\rFree Driving Mode - Press ESC to exit");
				reached = true;
				session->arrow_hide();
			}
		}
		arrow_point_at(checkpoint_index);
	}
	void out_of_time() override {
		if (reached) {
			return;
		}
		text("Out of Time!\rFinish 2 laps in under 2:30 to unlock Checkpoint Chase\rFree Driving Mode - Press ESC to exit");
		hide_checkpoints();
		session->arrow_hide();
		out = true;
	}
};

// ---- checkpoint_chase ----
class CheckpointChase : public CheckpointMode {
public:
	using CheckpointMode::CheckpointMode;
	bool uses_goal_counter() const override { return false; }
	void real_start() override {
		CheckpointMode::real_start();
		timer_set(10);
	}
	void out_of_time() override {
		session->sound()->play(4, PlayItem{ "stingloseloop" });
		text("Out of Time!\rHit all Checkpoints to unlock Stunt Rally");
		stop_text();
		stop_driving();
		finish_after(6000);
		session->arrow_hide();
	}
	void passed_checkpoint() override {
		text("Checkpoint!\r+10 Seconds");
		timer_add(10);
		checkpoint_index++;
		if (checkpoint_index > int(checkpoints.size())) {
			checkpoint_index = 1;
			laps++;
			if (laps > 1) {
				session->sound()->play(4, PlayItem{ "stingwin" });
				session->unlock(ModeId::STUNT);
				text("You did it!\rStunt Rally unlocked!\r");
				stop_text();
				stop_driving();
				finish_after(6000);
				session->arrow_hide();
			}
		}
		arrow_point_at(checkpoint_index);
	}
};

// ---- stunt_rally ----
class StuntRally : public GameMode {
public:
	using GameMode::GameMode;
	bool reached = false, out = false;
	void real_start() override {
		GameMode::real_start();
		timer_set(120);
		set_goal(2500);
	}
	void out_of_time() override {
		if (reached) {
			return;
		}
		out = true;
		session->get_car()->stop_driving();
		timer_stop();
		session->sound()->play(4, PlayItem{ "stingloseloop" });
		text("Out of Time!\rGet 2500 points to unlock Scavenger Hunt");
		stop_text();
		finish_after(6000);
	}
	void reached_goal() override {
		if (out) {
			return;
		}
		reached = true;
		session->sound()->play(4, PlayItem{ "stingwin" });
		session->unlock(ModeId::SCAVENGER);
		text("You did it!\rScavenger Hunt Unlocked!");
		stop_text();
		finish_after(6000);
		session->get_car()->stop_driving();
		timer_stop();
	}
};

// ---- prizes (scavenger_hunt / supersonic_challenge) ----
struct Prizes {
	std::vector<Node3D *> not_found;
	void create(DrivingSession *s) {
		for (const Vector3 &p : s->group_positions("prizes_", 139, false)) {
			not_found.push_back(s->clone_prop("scenes/prize.glb", p, 1.0));
		}
	}
	// Returns number of prizes collected this tick.
	int collect(DrivingSession *s, GameMode *m) {
		int n = 0;
		for (int i = int(not_found.size()) - 1; i >= 0; i--) {
			Node3D *prize = not_found[i];
			if (s->get_car()->xform.origin.distance_to(s->prop_position(prize)) < s->tuning().prize_radius) {
				not_found.erase(not_found.begin() + i);
				prize->set_visible(false);
				s->sound()->play(4, PlayItem{ "stinghiddentreasure" });
				m->add_points(int(s->tuning().prize_points));
				m->text("+100 Points");
				n++;
			}
		}
		return n;
	}
};

class ScavengerHunt : public GameMode {
public:
	using GameMode::GameMode;
	Prizes prizes;
	bool reached = false, out = false;
	void real_start() override {
		GameMode::real_start();
		prizes.create(session);
		set_goal(3000);
		timer_set(5 * 60);
	}
	void stunt_event(int) override {}
	void havok_update() override {
		GameMode::havok_update();
		if (stopping) {
			return;
		}
		prizes.collect(session, this);
	}
	void out_of_time() override {
		if (reached) {
			return;
		}
		out = true;
		session->sound()->play(4, PlayItem{ "stingloseloop" });
		text("Out of Time!\rGet 3000 points to unlock Supersonic Challenge\rPress ESC to exit");
		stop_text();
		session->get_car()->stop_driving();
		timer_stop();
		finish_after(6000);
	}
	void reached_goal() override {
		if (out) {
			return;
		}
		reached = true;
		session->sound()->play(4, PlayItem{ "stingwin" });
		session->unlock(ModeId::SUPERSONIC);
		text("You did it!\rSupersonic Challenge Unlocked!");
		stop_text();
		session->get_car()->stop_driving();
		timer_stop();
		finish_after(6000);
	}
};

class SupersonicChallenge : public CheckpointMode {
public:
	using CheckpointMode::CheckpointMode;
	Prizes prizes;
	void real_start() override {
		CheckpointMode::real_start();
		prizes.create(session);
		timer_set(5 * 60);
		set_goal(15000);
	}
	void havok_update() override {
		CheckpointMode::havok_update();
		if (stopping) {
			return;
		}
		prizes.collect(session, this);
	}
	void stunt_event(int p) override {
		text(stunt_text(p) + "\r+" + String::num_int64(p) + " Points!");
		add_points(p);
	}
	void passed_checkpoint() override {
		checkpoint_index++;
		if (checkpoint_index > int(checkpoints.size())) {
			checkpoint_index = 1;
			laps++;
			session->arrow_hide();
			hide_checkpoints();
			if (laps > 0 && reached) {
				win();
				return;
			}
		}
		arrow_point_at(checkpoint_index);
		timer_add(2);
		add_points(500);
		text("Checkpoint!\r+2 Seconds! +500 Points!");
	}
	void out_of_time() override {
		if (reached && laps > 1) {
			return;
		}
		out = true;
		session->sound()->play(4, PlayItem{ "stingloseloop" });
		text("Out of Time!\rGet 15000 Points and 1 Lap to win the game.\rPress ESC to exit");
		stop_text();
		session->get_car()->stop_driving();
		timer_stop();
		finish_after(6000);
	}
	void reached_goal() override {
		if (out) {
			return;
		}
		reached = true;
		if (laps > 0) {
			win();
		}
	}
	void win() {
		session->sound()->play(4, PlayItem{ "stingwin" });
		text("\rCongratulations! You've won the Supersonic Challenge!\rPress ESC to return to the main menu.");
		stop_text();
		session->get_car()->stop_driving();
		timer_stop();
		finish_after(6000);
	}
};

// ============================================================ Session

SoundSystem *DrivingSession::sound() const {
	return game->get_sound();
}

real_t DrivingSession::real_ms() const {
	return Time::get_singleton()->get_ticks_usec() / 1000.0;
}

void DrivingSession::unlock(ModeId id) {
	game->unlock_mode(id);
}

void DrivingSession::setup(SupersonicGame *p_game, ModeId mode_id) {
	game = p_game;
	set_name("driving");
	Variant tj = Assets::json("tuning.json");
	if (tj.get_type() == Variant::DICTIONARY) {
		tune.apply(tj);
	}
	uint64_t t0 = Time::get_singleton()->get_ticks_msec();
	build_world();
	UtilityFunctions::print("[ssrc] world built in ", Time::get_singleton()->get_ticks_msec() - t0, " ms");
	hud = std::make_unique<Hud>(this);
	car = std::make_unique<Car>(this, this);
	cam = std::make_unique<ChaseCamera>(this, game->settings().camera_mode == 1);

	// Arrow (cloned from arrow2, drawn over the scene).
	MeshInstance3D *amesh = nullptr;
	(void)prop_model_xform("scenes/arrow.glb", &amesh);
	if (amesh) {
		arrow = amesh;
		add_child(arrow);
	}

	switch (mode_id) {
		case ModeId::FREE: mode = std::make_unique<FreeDriving>(this, mode_id); break;
		case ModeId::CHECKPOINT: mode = std::make_unique<CheckpointChase>(this, mode_id); break;
		case ModeId::STUNT: mode = std::make_unique<StuntRally>(this, mode_id); break;
		case ModeId::SCAVENGER: mode = std::make_unique<ScavengerHunt>(this, mode_id); break;
		default: mode = std::make_unique<SupersonicChallenge>(this, mode_id); break;
	}
	mode->real_start();
	arrow_visible = arrow != nullptr && dynamic_cast<CheckpointMode *>(mode.get()) != nullptr;
	if (arrow) {
		arrow->set_visible(arrow_visible);
	}
	hud->layout();
	running = false;
	UtilityFunctions::print("[ssrc] session ready in ", Time::get_singleton()->get_ticks_msec() - t0, " ms");
}

void DrivingSession::first_frame() {
	running = true;
	cam->update();
	cam->camera->reset_physics_interpolation();
}

void DrivingSession::request_exit() {
	if (finished) {
		return;
	}
	mode->stop_driving();
	finish();
}

void DrivingSession::finish() {
	if (finished) {
		return;
	}
	finished = true;
	running = false;
	if (car) {
		car->shutdown_sound();
	}
	game->call_deferred("session_finished");
}

void DrivingSession::_exit_tree() {
	mode.reset();
	car.reset();
	cam.reset();
	hud.reset();
}

Transform3D DrivingSession::prop_model_xform(const String &scene_rel, MeshInstance3D **mesh_out) {
	*mesh_out = nullptr;
	Node3D *scene = Assets::scene(scene_rel);
	if (!scene) {
		return Transform3D();
	}
	TypedArray<Node> found = scene->find_children("*", "MeshInstance3D", true, false);
	Transform3D local;
	if (found.size() > 0) {
		MeshInstance3D *m = Object::cast_to<MeshInstance3D>(found[0]);
		local = m->get_transform(); // raw Director model transform
		String tex_dir = scene_rel.get_file().get_basename() + "_textures";
		const Settings &st = game->settings();
		bool overlay = scene_rel.contains("arrow");
		// The arrow is drawn by its own camera whose rootNode is the arrow, so
		// no scene lights reach it: only its emissive colour shows.
		apply_director_materials(m, tex_dir, overlay ? DirectorLights() : lights, overlay ? 0 : st.lighting, st.texture_filter, overlay);
		m->get_parent()->remove_child(m);
		m->set_transform(Transform3D());
		*mesh_out = m;
	}
	scene->queue_free();
	return local;
}

Node3D *DrivingSession::clone_prop(const String &scene_rel, const Vector3 &pos_d, real_t scale) {
	MeshInstance3D *m = nullptr;
	Transform3D local = prop_model_xform(scene_rel, &m);
	Node3D *holder = memnew(Node3D);
	add_child(holder);
	if (m) {
		holder->add_child(m);
	}
	Transform3D d = local;
	d.basis = d.basis.scaled(Vector3(scale, scale, scale));
	d.origin = pos_d;
	holder->set_meta("director_xform", d);
	holder->set_global_transform(director_to_godot_xform(d));
	return holder;
}

Vector3 DrivingSession::prop_position(Node3D *n) const {
	Transform3D d = n->get_meta("director_xform");
	return d.origin;
}

void DrivingSession::spin_prop(Node3D *n, real_t deg) {
	Transform3D d = n->get_meta("director_xform");
	d.basis = rot_z_deg(deg) * d.basis;
	n->set_meta("director_xform", d);
	n->set_global_transform(director_to_godot_xform(d));
}

std::vector<Vector3> DrivingSession::group_positions(const String &prefix, int max_index, bool stop_at_gap) {
	std::vector<Vector3> out;
	Node *world = get_node_or_null(NodePath("world"));
	for (int i = 1; i <= max_index; i++) {
		String name = prefix + String::num_int64(i);
		Node3D *g = Object::cast_to<Node3D>(find_child_ci(world, name));
		if (!g) {
			if (stop_at_gap) {
				break;
			}
			continue;
		}
		out.push_back(g2d(g->get_global_position()));
	}
	return out;
}

void DrivingSession::arrow_point_at(const Vector3 &target_d) {
	arrow_target = target_d;
}

void DrivingSession::arrow_hide() {
	arrow_visible = false;
	if (arrow) {
		arrow->set_visible(false);
	}
}

RayHit DrivingSession::raycast(const Vector3 &from_d, const Vector3 &dir_d, real_t length_d, bool) {
	RayHit r;
	Ref<World3D> w = get_world_3d();
	if (w.is_null()) {
		return r;
	}
	PhysicsDirectSpaceState3D *space = w->get_direct_space_state();
	Vector3 from = d2g(from_d);
	Vector3 dir = d2g_dir(dir_d).normalized();
	Ref<PhysicsRayQueryParameters3D> q = PhysicsRayQueryParameters3D::create(from, from + dir * (length_d * WORLD_SCALE), LAYER_RAY);
	q->set_hit_back_faces(true);
	q->set_collide_with_areas(false);
	Dictionary hit = space->intersect_ray(q);
	if (hit.is_empty()) {
		return r;
	}
	r.hit = true;
	r.position = g2d(hit["position"]);
	Vector3 n = hit["normal"];
	if (n.dot(dir) > 0) {
		n = -n; // face the ray origin like Director's #isectNormal
	}
	r.normal = g2d_dir(n).normalized();
	return r;
}

void DrivingSession::load_lights() {
	lights = DirectorLights();
	Dictionary lights_json = Assets::json("scenes/world_lights.json");
	Array arr = lights_json.get("lights", Array());
	for (int i = 0; i < arr.size(); i++) {
		Dictionary l = arr[i];
		if (!bool(l.get("enabled", true))) {
			continue;
		}
		Array c = l.get("color", Array());
		Color col = c.size() >= 3 ? Color(c[0], c[1], c[2]) : Color(1, 1, 1);
		String type = l.get("type", "");
		if (type == "ambient") {
			lights.ambient += col;
		} else if (type == "directional") {
			Array m = l.get("world_matrix_director", Array());
			if (m.size() != 16) {
				continue;
			}
			Vector3 z_axis(m[8], m[9], m[10]);
			lights.dirs_g.push_back(d2g_dir(-z_axis.normalized()).normalized());
			lights.colors.push_back(col);
		}
	}
}

void DrivingSession::build_world() {
	Node3D *world = Assets::scene("scenes/world.glb");
	ERR_FAIL_NULL(world);
	world->set_name("world");
	add_child(world);
	const Settings &st = game->settings();
	load_lights();
	apply_director_materials(world, "world_textures", lights, st.lighting, st.texture_filter);

	// Visual geometry for modelsUnderRay (wheels, camera, shadow, respawn).
	PackedVector3Array ray_faces;
	TypedArray<Node> meshes = world->find_children("*", "MeshInstance3D", true, false);
	for (int i = 0; i < meshes.size(); i++) {
		MeshInstance3D *mi = Object::cast_to<MeshInstance3D>(meshes[i]);
		Ref<Mesh> mesh = mi->get_mesh();
		if (mesh.is_null()) {
			continue;
		}
		String nm = String(mi->get_name()).to_lower();
		if (nm == "car" || nm == "arrow") {
			continue; // helper models the original removes or never rays against
		}
		Transform3D gx = mi->get_global_transform();
		PackedVector3Array f = mesh->get_faces();
		for (int k = 0; k < f.size(); k++) {
			ray_faces.push_back(gx.xform(f[k]));
		}
	}
	StaticBody3D *ray_body = memnew(StaticBody3D);
	ray_body->set_name("ray_world");
	ray_body->set_collision_layer(LAYER_RAY);
	ray_body->set_collision_mask(0);
	Ref<ConcavePolygonShape3D> ray_shape;
	ray_shape.instantiate();
	ray_shape->set_backface_collision_enabled(true);
	ray_shape->set_faces(ray_faces);
	CollisionShape3D *rcs = memnew(CollisionShape3D);
	rcs->set_shape(ray_shape);
	ray_body->add_child(rcs);
	add_child(ray_body);

	// Havok collision world.
	Dictionary phys = Assets::json("physics/collision.json");
	Array bodies = phys.get("bodies", Array());
	StaticBody3D *coll = memnew(StaticBody3D);
	coll->set_name("havok_world");
	coll->set_collision_layer(LAYER_WORLD);
	coll->set_collision_mask(0);
	Ref<PhysicsMaterial> pm;
	pm.instantiate();
	pm->set_friction(0.3);
	pm->set_bounce(tune.world_restitution);
	coll->set_physics_material_override(pm);
	for (int i = 0; i < bodies.size(); i++) {
		Dictionary b = bodies[i];
		Array f = b["faces"];
		PackedVector3Array faces;
		faces.resize(f.size() / 3);
		for (int k = 0; k + 2 < f.size(); k += 3) {
			faces.set(k / 3, Vector3(f[k], f[k + 1], f[k + 2]));
		}
		Ref<ConcavePolygonShape3D> shape;
		shape.instantiate();
		// Two-sided: the HKE triangles are wound the opposite way to
		// Godot's front faces (and some meshes mix both), so one-sided
		// shapes let the car through floors, walls, ramps and stairs.
		shape->set_backface_collision_enabled(true);
		shape->set_faces(faces);
		CollisionShape3D *cs = memnew(CollisionShape3D);
		cs->set_name(String(b["name"]));
		cs->set_shape(shape);
		coll->add_child(cs);
	}
	add_child(coll);

	// Godot lights only drive the "Enhanced" look; "Original" shading is
	// computed in the material from the same Director lights.
	if (st.lighting == 1) {
		for (size_t i = 0; i < lights.dirs_g.size(); i++) {
			DirectionalLight3D *dl = memnew(DirectionalLight3D);
			dl->set_color(lights.colors[i]);
			dl->set_param(Light3D::PARAM_ENERGY, 1.0);
			Vector3 dir_g = lights.dirs_g[i];
			Vector3 up = Math::abs(dir_g.y) > 0.99 ? Vector3(1, 0, 0) : Vector3(0, 1, 0);
			dl->look_at_from_position(Vector3(), dir_g, up);
			dl->set_shadow(dir_g.y < -0.5);
			add_child(dl);
		}
	}
	Color ambient = lights.ambient;
	WorldEnvironment *we = memnew(WorldEnvironment);
	Ref<Environment> env;
	env.instantiate();
	env->set_background(Environment::BG_COLOR);
	env->set_bg_color(Color(0, 0, 0));
	env->set_ambient_source(Environment::AMBIENT_SOURCE_COLOR);
	env->set_ambient_light_color(ambient);
	env->set_ambient_light_energy(1.0);
	env->set_tonemapper(Environment::TONE_MAPPER_LINEAR);
	if (st.lighting == 1) {
		env->set_ssao_enabled(true);
		env->set_ssao_intensity(1.0);
		env->set_glow_enabled(true);
		env->set_glow_intensity(0.3);
	}
	we->set_environment(env);
	add_child(we);
}

void DrivingSession::logic_tick() {
	Car *c = car.get();
	c->read_state();
	c->after_physics_contacts();

	Input *inp = Input::get_singleton();
	if (inp->is_action_pressed("back") && !finished) {
		request_exit();
		return;
	}
	DriveInput in;
	in.up = inp->is_action_pressed("accelerate");
	in.down = inp->is_action_pressed("brake");
	in.left = inp->is_action_pressed("steer_left");
	in.right = inp->is_action_pressed("steer_right");
	in.handbrake = inp->is_action_pressed("handbrake");
	real_t axis = inp->get_axis("steer_left_analog", "steer_right_analog");
	in.steer_axis = Math::abs(axis) > 0.15 ? axis : 0.0;

	static int dbg = OS::get_singleton()->get_environment("SSRC_DEBUG_CAR").to_int();
	if (dbg > 0 && logic_ticks < dbg) {
		String w;
		for (Wheel *wh : c->wheels) {
			w += wh->on_ground ? String("G") + String::num(wh->load, 0) + " " : String("- ");
		}
		UtilityFunctions::print("[car] t=", logic_ticks, " sim=", String::num(sim_seconds, 3), " pos=", c->xform.origin, " vel=", c->linear_velocity, " w=", c->angular_velocity, " up=", c->xform.z_axis(), " wheels ", w, " contacts=",
				c->get_body()->get_contact_count());
	}
	logic_ticks++;
	cam->update(); // drivingManager.update: havok step already done, then camera
	c->clear_forces();
	c->update(in);
	mode->havok_update();
	mode->stunt_update();

	// actorList stepFrame
	c->step_frame();
	mode->step_frame();
	static bool probe = OS::get_singleton()->get_environment("SSRC_COLLISION_PROBE") != "";
	if (probe && logic_ticks == 2) {
		run_collision_probe();
	}
	static String pick = OS::get_singleton()->get_environment("SSRC_PICK");
	if (!pick.is_empty() && logic_ticks == 200) {
		// Debug: which model/shader is under screen point x,y (640x480 space)?
		for (const String &pt : pick.split(";")) {
			Vector2 sp(pt.get_slice(",", 0).to_float(), pt.get_slice(",", 1).to_float());
			Vector2 vs = get_viewport()->get_visible_rect().size;
			Vector2 px = sp * (vs.y / 480.0) + Vector2((vs.x - 640 * vs.y / 480.0) / 2, 0);
			Vector3 o = cam->camera->project_ray_origin(px), d = cam->camera->project_ray_normal(px);
			real_t best = 1e9;
			String best_name;
			TypedArray<Node> ms = find_children("*", "MeshInstance3D", true, false);
			for (int i = 0; i < ms.size(); i++) {
				MeshInstance3D *mi = Object::cast_to<MeshInstance3D>(ms[i]);
				if (!mi->is_visible_in_tree() || mi->get_mesh().is_null()) continue;
				Transform3D gx = mi->get_global_transform();
				Ref<Mesh> mesh = mi->get_mesh();
				for (int si = 0; si < mesh->get_surface_count(); si++) {
					Array arrs = mesh->surface_get_arrays(si);
					PackedVector3Array v = arrs[Mesh::ARRAY_VERTEX];
					PackedInt32Array idx = arrs[Mesh::ARRAY_INDEX];
					for (int k = 0; k + 2 < idx.size(); k += 3) {
						Variant r = Geometry3D::get_singleton()->ray_intersects_triangle(o, d, gx.xform(v[idx[k]]), gx.xform(v[idx[k + 1]]), gx.xform(v[idx[k + 2]]));
						if (r.get_type() == Variant::VECTOR3) {
							real_t dist = o.distance_to(r);
							if (dist < best) {
								best = dist;
								Ref<Material> mm = mesh->surface_get_material(si);
								Dictionary ex = mm.is_valid() && mm->has_meta("extras") ? Dictionary(mm->get_meta("extras")) : Dictionary();
								Dictionary nx = mi->has_meta("extras") ? Dictionary(mi->get_meta("extras")) : Dictionary();
								best_name = String(mi->get_name()) + " surf " + String::num_int64(si) + " shader=" + String(ex.get("w3d_shader", "?")) + " lm=" + String(nx.get("lightmap", "")) + " " + JSON::stringify(ex);
							}
						}
					}
				}
			}
			UtilityFunctions::print("[pick] ", pt, " -> ", best_name);
		}
	}
	if (arrow && arrow_visible) {
		Transform3D cg = cam->camera->get_global_transform();
		Vector3 pos_g = cg.xform(Vector3(0, -12, -50) * WORLD_SCALE);
		Vector3 pos_d = g2d(pos_g);
		Vector3 target = arrow_target;
		target.z = pos_d.z;
		Vector3 y = target - pos_d;
		if (y.length() > 1e-4) {
			y.normalize();
			Vector3 up = g2d_dir(cg.basis.get_column(1)).normalized();
			Vector3 x = y.cross(up).normalized();
			Vector3 z = x.cross(y).normalized();
			Transform3D d(Basis(x * 0.05, y * 0.05, z * 0.05), pos_d);
			// The arrow had its own camera ("arrowcam", Director's default
			// 30 degree field of view) layered over the main one. Emulate that
			// projection by compressing the arrow along the view axis.
			real_t k = Math::tan(Math::deg_to_rad(cam->camera->get_fov() * 0.5)) / Math::tan(Math::deg_to_rad(15.0));
			Transform3D squash(Basis().scaled(Vector3(1, 1, 1.0 / k)), Vector3());
			arrow->set_global_transform(cg * squash * cg.affine_inverse() * director_to_godot_xform(d));
		}
	}
}

void DrivingSession::_physics_process(double delta) {
	if (!running || finished) {
		return;
	}
	sim_seconds += delta;
	int substeps = MAX(1, game->settings().physics_substeps);
	if (tick_counter % substeps == 0) {
		logic_tick();
	}
	tick_counter++;
	if (car && !finished) {
		car->flush_forces();
	}
}

void DrivingSession::_process(double) {
	static uint64_t last = 0;
	uint64_t now = Time::get_singleton()->get_ticks_usec();
	real_t dt_ms = last ? (now - last) / 1000.0 : 0;
	last = now;
	if (hud) {
		hud->update(dt_ms);
		hud->layout();
	}
}


// Test hook (SSRC_COLLISION_PROBE=1): sweep a car-sized box at known
// surfaces of the Havok world and print PASS/FAIL for each, then quit.
// Every surface must stop the box from the side the player meets it.
void DrivingSession::run_collision_probe() {
	struct Probe {
		const char *name;
		Vector3 from, motion; // Godot metres
	};
	const Probe probes[] = {
		{ "lower floor from above", Vector3(0, 2, -100), Vector3(0, -4, 0) },
		{ "upper floor from above", Vector3(96.3, 10, 144.7), Vector3(0, -4, 0) },
		{ "outer wall from inside", Vector3(-120, 1, -100), Vector3(-40, 0, 0) },
		{ "Big Ramp from above", Vector3(65, 30, -95), Vector3(0, -40, 0) },
		{ "stairs step from above", Vector3(-17, 12, 70), Vector3(0, -20, 0) },
		{ "shelf side", Vector3(75, 1, -22), Vector3(15, 0, 0) },
		{ "shelf top from above", Vector3(88, 20, -22), Vector3(0, -10, 0) },
		{ "dinosaur from above", Vector3(-38, 45, 117), Vector3(0, -40, 0) },
	};
	Ref<BoxShape3D> box;
	box.instantiate();
	box->set_size(Vector3(15, 10, 30) * WORLD_SCALE);
	PhysicsDirectSpaceState3D *space = get_world_3d()->get_direct_space_state();
	int failed = 0;
	for (const Probe &p : probes) {
		Ref<PhysicsShapeQueryParameters3D> q;
		q.instantiate();
		q->set_shape(box);
		q->set_collision_mask(LAYER_WORLD);
		q->set_transform(Transform3D(Basis(), p.from));
		q->set_motion(p.motion);
		PackedFloat32Array r = space->cast_motion(q);
		bool hit = r.size() > 0 && r[0] < 1.0;
		failed += hit ? 0 : 1;
		UtilityFunctions::print("[probe] ", hit ? "PASS " : "FAIL ", p.name, " stopped at ", p.from + p.motion * (r.size() > 0 ? r[0] : 1.0));
	}
	UtilityFunctions::print("[probe] ", failed == 0 ? "ALL PASS" : String::num_int64(failed) + " FAILED");
	get_tree()->quit(failed == 0 ? 0 : 1);
}

} // namespace ssrc
