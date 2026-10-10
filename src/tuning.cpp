#include "tuning.h"

#include <godot_cpp/variant/variant.hpp>

namespace ssrc {

FrameTiming frame_timing(real_t frame_rate) {
	// drivingManager.update: chunks of at most 20 ms, the remainder first.
	int frame_ms = int(Math::round(1000.0 / MAX(frame_rate, 1.0)));
	std::vector<int> chunks;
	int rest = MIN(frame_ms, 100);
	while (rest > 20) {
		chunks.insert(chunks.begin(), 20);
		rest -= 20;
	}
	chunks.insert(chunks.begin(), rest);
	// havokManager.update: clamp to 3..15 ms, step 2.5x that. Work in
	// half-milliseconds of simulation so the base tick is exact.
	std::vector<int> sim_half_ms;
	int total = 0, g = 0;
	for (int c : chunks) {
		int h = CLAMP(c, 3, 15) * 5; // 2.5x in half-ms
		sim_half_ms.push_back(h);
		total += h;
		int a = g, b = h;
		while (b) {
			int t = a % b;
			a = b;
			b = t;
		}
		g = a;
	}
	FrameTiming ft;
	ft.frame_rate = 1000.0 / frame_ms;
	for (int h : sim_half_ms) {
		ft.chunk_ticks.push_back(h / g);
	}
	ft.ticks_per_frame = total / g;
	ft.tick_sim = g / 2000.0;
	ft.sim_per_real = (total / 2000.0) / (frame_ms / 1000.0);
	return ft;
}

#define SSRC_TUNING_FIELDS(X) \
	X(gravity_factor)        \
	X(frame_rate)            \
	X(car_mass)              \
	X(car_friction)          \
	X(car_restitution)       \
	X(world_restitution)     \
	X(lift_body)             \
	X(com_shift_z)           \
	X(max_angular_on_collision) \
	X(force_divider)         \
	X(torque_divider_pitch)  \
	X(torque_divider_roll)   \
	X(torque_divider_yaw)    \
	X(angular_damp)          \
	X(start_x)               \
	X(start_y)               \
	X(start_z)               \
	X(wheel_x)               \
	X(wheel_y)               \
	X(wheel_z)               \
	X(wheel_radius)          \
	X(rest_length)           \
	X(min_spring_length)     \
	X(ray_start_offset)      \
	X(spring_factor)         \
	X(damp_factor)           \
	X(damp_speed_limit)      \
	X(drive_force)           \
	X(reverse_force)         \
	X(brake_force)           \
	X(handbrake_min_speed)   \
	X(rolling_drag)          \
	X(side_drag)             \
	X(side_grip_per_load)    \
	X(side_grip_traction_bonus) \
	X(skid_drive_factor)     \
	X(skid_drag_factor)      \
	X(skid_grip_factor)      \
	X(handbrake_rear_grip)   \
	X(steer_max_deg)         \
	X(steer_step_deg)        \
	X(visual_steer_factor)   \
	X(wheel_visual_drop)     \
	X(camera_fov)            \
	X(camera_distance)       \
	X(camera_height)         \
	X(camera_lerp)           \
	X(camera_follow_min_speed) \
	X(checkpoint_radius)     \
	X(prize_radius)          \
	X(prize_points)          \
	X(sim_seconds_per_clock_second) \
	X(flip_recover_seconds)

void Tuning::apply(const Dictionary &d) {
#define X(name)                     \
	if (d.has(String(#name))) {             \
		name = real_t(double(d.get(String(#name), Variant(double(name))))); \
	}
	SSRC_TUNING_FIELDS(X)
#undef X
}

Dictionary Tuning::to_dict() const {
	Dictionary d;
#define X(name) d[String(#name)] = name;
	SSRC_TUNING_FIELDS(X)
#undef X
	return d;
}

} // namespace ssrc
