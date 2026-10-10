// A driving session: the 3D world, the car, the camera, the HUD and the
// rules of one game mode. Ports drivingManager, havokManager, camera,
// gameTypeManager and its subclasses, timers, points counters, the stunt
// detector, pickupManager, checkpoints, prizes and the arrow.
#pragma once

#include "car.h"
#include "materials.h"
#include "tuning.h"
#include "util.h"

#include <godot_cpp/classes/camera3d.hpp>
#include <godot_cpp/classes/canvas_layer.hpp>
#include <godot_cpp/classes/control.hpp>
#include <godot_cpp/classes/font.hpp>
#include <godot_cpp/classes/label.hpp>
#include <godot_cpp/classes/mesh_instance3d.hpp>
#include <godot_cpp/classes/node3d.hpp>
#include <godot_cpp/classes/v_box_container.hpp>
#include <memory>
#include <vector>

namespace ssrc {

class SupersonicGame;
class SoundSystem;

enum class ModeId { FREE = 0, CHECKPOINT, STUNT, SCAVENGER, SUPERSONIC, COUNT };
const char *mode_script_name(ModeId id);

class DrivingSession;

// ---- HUD (the 'insceneoverlay' / '3d_textsprite' family, drawn in 2D) ----
class Hud {
public:
	explicit Hud(Node *parent);
	~Hud();
	void set_timer(const String &mmss);
	void show_timer(bool on);
	void setup_points(bool with_goal);
	void set_points(const String &text, bool goal_reached);
	void set_message(const String &text); // lines: 24 / 18 / 12 pt
	void fade_out_message();
	void update(real_t real_ms); // fades
	void layout();

	CanvasLayer *layer = nullptr;

private:
	Ref<Font> font_regular, font_bold_italic;
	Control *root = nullptr;
	Label *timer_caption = nullptr;
	std::vector<Label *> timer_digits;
	Label *points_caption = nullptr;
	std::vector<Label *> points_digits;
	bool points_goal = false;
	VBoxContainer *message = nullptr;
	real_t message_blend = 0, message_target = 0;
	Label *make_label(int size, bool bold_italic, const Color &c);
	void place_digits(std::vector<Label *> &digits, int count, real_t x0, real_t anchor);
};

// ---- chase camera ----
class ChaseCamera {
public:
	ChaseCamera(DrivingSession *s, bool from_a_distance);
	void update();
	void car_stopped_driving() { orbit = true; }
	Camera3D *camera = nullptr;
	Vector3 position; // Director space

private:
	DrivingSession *session;
	bool distance_mode;
	bool following = false;
	bool orbit = false;
	Vector3 wanna_be;
};

// ---- game modes (gameTypeManager and subclasses) ----
class GameMode {
public:
	GameMode(DrivingSession *s, ModeId id);
	virtual ~GameMode() = default;
	virtual void real_start();
	virtual void havok_update(); // per logic tick
	virtual void step_frame();
	virtual void out_of_time() {}
	virtual void reached_goal() {}
	virtual void stunt_event(int points);
	virtual bool uses_goal_counter() const { return true; }
	virtual void passed_checkpoint() {}

	void stop_driving();
	void finish_after(real_t ms); // script("timeout").new(#destroy, 6000)
	void text(const String &t);
	void stop_text() { text_stopped = true; }

	ModeId id;
	DrivingSession *session;
	bool stopping = false;

	// chasetimer
	void timer_set(real_t seconds);
	void timer_add(real_t seconds);
	int timer_get();
	void timer_stop();
	real_t timer_start = 0, timer_total = 20;
	bool timer_stopped = false, timer_out = false;
	int timer_stop_value = 0;
	int last_oink = 10;

	// points counter
	void add_points(int p);
	void set_goal(int g);
	int points = 0, goal = 500;
	bool goal_reached = false;
	void refresh_points();

	// textmessagemanager
	bool text_stopped = false;
	real_t text_started_ms = -1;

	// stunt detector
	bool jump_in_progress = false;
	real_t jump_start_time = 0, jump_max_height = 0, angular_distance = 0;
	Vector3 jump_start_pos;
	void stunt_update();

	// pickupManager
	bool upside_down = false;
	real_t upside_down_start = 0;
	void pickup_update();

	real_t finish_at_ms = -1;
};

class CheckpointMode : public GameMode {
public:
	using GameMode::GameMode;
	void real_start() override;
	void havok_update() override;
	void hide_checkpoints();
	void show_this_checkpoint(int index);
	void arrow_point_at(int index);

	std::vector<Node3D *> checkpoints;
	std::vector<Vector3> checkpoint_pos; // Director space
	int checkpoint_index = 1;
	bool hidden = false;
	int laps = 0;
	bool reached = false, out = false;
};

class DrivingSession : public Node3D {
	GDCLASS(DrivingSession, Node3D)

public:
	static constexpr uint32_t LAYER_WORLD = 1; // Havok collision meshes
	static constexpr uint32_t LAYER_RAY = 2; // visual geometry for modelsUnderRay
	static constexpr uint32_t LAYER_CAR = 4;

	void setup(SupersonicGame *game, ModeId mode);
	void first_frame();
	void request_exit(); // ESC
	void finish(); // gameTypeManager.destroy -> back to level select

	const Tuning &tuning() const { return tune; }
	SoundSystem *sound() const;
	SupersonicGame *get_game() const { return game; }
	const DirectorLights &get_lights() const { return lights; }
	Node3D *get_world_root() { return this; }
	Car *get_car() const { return car.get(); }
	ChaseCamera *get_camera() const { return cam.get(); }
	Hud *get_hud() const { return hud.get(); }
	real_t sim_time() const { return sim_seconds; }
	real_t real_ms() const;

	RayHit raycast(const Vector3 &from_d, const Vector3 &dir_d, real_t length_d, bool any_length);

	// Props.
	Node3D *clone_prop(const String &scene_rel, const Vector3 &pos_d, real_t scale);
	std::vector<Vector3> group_positions(const String &prefix, int max_index, bool stop_at_gap);
	void arrow_point_at(const Vector3 &target_d);
	void arrow_hide();
	void spin_prop(Node3D *n, real_t deg);
	Vector3 prop_position(Node3D *n) const;

	void set_drive_input(const DriveInput &in) { input = in; }
	void unlock(ModeId id);

	void _physics_process(double delta) override;
	void _process(double delta) override;
	void _exit_tree() override;

	// Test hook: the car's physics state in Director units (inches,
	// Z up), comparable with the original's Havok state read from memory.
	Dictionary get_car_state() const;
	// Test hook: put the car at rest at pos (Director units), heading
	// about +Z in degrees (0 faces +Y).
	void test_spawn(const Vector3 &pos, real_t heading_deg);

protected:
	static void _bind_methods() {
		ClassDB::bind_method(D_METHOD("get_car_state"), &DrivingSession::get_car_state);
		ClassDB::bind_method(D_METHOD("test_spawn", "pos", "heading_deg"), &DrivingSession::test_spawn);
	}

private:
	SupersonicGame *game = nullptr;
	Tuning tune;
	DirectorLights lights;
	std::unique_ptr<Car> car;
	std::unique_ptr<ChaseCamera> cam;
	std::unique_ptr<Hud> hud;
	std::unique_ptr<GameMode> mode;
	DriveInput input;
	real_t sim_seconds = 0;
	bool running = false;
	bool finished = false;
	int tick_counter = 0;
	int logic_ticks = 0;
	Node3D *arrow = nullptr;
	bool arrow_visible = false;
	Vector3 arrow_target;
	Transform3D prop_model_xform(const String &scene_rel, MeshInstance3D **mesh_out);

	void build_world();
	void run_collision_probe();
	void load_lights();
	void logic_tick();
};

Transform3D director_to_godot_xform(const Transform3D &d);

} // namespace ssrc
