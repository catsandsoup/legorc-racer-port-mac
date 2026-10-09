// Every gameplay constant from the original Lingo, in one moddable table.
// Defaults are the 2003 values; tuning.json (in data or any mod) overrides
// any of them by name.
#pragma once

#include <godot_cpp/core/math_defs.hpp>
#include <godot_cpp/variant/dictionary.hpp>

namespace ssrc {

using namespace godot;

struct Tuning {
	// havokManager
	real_t gravity_factor = 0.2; // pHavok.gravity * 0.2
	real_t sim_speed = 2.25; // sim seconds per real second (step clamp 0.015 * 2.5 per 60 Hz update)
	real_t logic_hz = 60.0; // drivingManager update rate (<= 20 ms chunks)
	// car
	real_t car_mass = 3.0;
	real_t car_friction = 10.0;
	real_t car_restitution = 0.01;
	real_t world_restitution = 0.0;
	real_t lift_body = 2.0;
	real_t com_shift_z = -11.5; // shiftCenterOfMass(0,0,-11.5 - liftBody)
	real_t max_angular_on_collision = 4.0;
	// How the Havok Xtra integrated forces applied from Lingo: they only
	// survive the first micro-step of step(dt, 7), so a force F changes
	// velocity by F*dt/(6*7*m). Measured against Director by the DirPlayer
	// project (havok_physics.rs); torque dividers are their calibrated values.
	real_t force_divider = 42.0;
	real_t torque_divider_pitch_roll = 434.0;
	real_t torque_divider_yaw = 158.76;
	real_t angular_damp = 0.0;
	real_t start_x = 3790.17, start_y = -5696.19, start_z = 338.6598;
	real_t wheel_x = 9.5, wheel_y = 17.0, wheel_z = -5.0;
	// wheel
	real_t wheel_radius = 25.0;
	real_t rest_length = 30.0;
	real_t min_spring_length = 20.0;
	real_t ray_start_offset = 25.0;
	real_t spring_factor = 10.0; // mass * |g| / 4 * 10
	real_t damp_factor = 0.09;
	real_t damp_speed_limit = 30.0;
	real_t drive_force = 6300.0;
	real_t reverse_force = 4000.0;
	real_t brake_force = 10000.0;
	real_t handbrake_min_speed = 10.0;
	real_t rolling_drag = 6.0;
	real_t side_drag = 150.0;
	real_t side_grip_per_load = 3.0;
	real_t side_grip_traction_bonus = 100.0;
	real_t skid_drive_factor = 0.5;
	real_t skid_drag_factor = 0.5;
	real_t skid_grip_factor = 0.75;
	real_t handbrake_rear_grip = 0.3;
	real_t steer_max_deg = 5.0;
	real_t steer_step_deg = 1.0;
	real_t visual_steer_factor = 3.0;
	real_t wheel_visual_drop = 5.2;
	// camera
	real_t camera_fov = 70.0;
	real_t camera_distance = 50.0;
	real_t camera_height = 10.0;
	real_t camera_lerp = 0.1;
	real_t camera_follow_min_speed = 10.0;
	// game rules
	real_t checkpoint_radius = 300.0;
	real_t prize_radius = 100.0;
	real_t prize_points = 100.0;
	real_t sim_seconds_per_clock_second = 3.0; // timers show simTime / 3
	real_t flip_recover_seconds = 8.0;

	void apply(const Dictionary &d);
	Dictionary to_dict() const;
};

} // namespace ssrc
