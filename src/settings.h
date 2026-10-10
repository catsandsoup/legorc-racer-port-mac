// Player settings (user://settings.cfg) and unlock progress (user://progress.cfg).
#pragma once

#include <godot_cpp/core/math_defs.hpp>
#include <godot_cpp/variant/string.hpp>

namespace ssrc {

using namespace godot;

struct Settings {
	// Display
	int window_mode = 0; // 0 windowed, 1 fullscreen, 2 exclusive fullscreen
	int window_w = 1440, window_h = 1080;
	int vsync = 1; // 0 off, 1 on, 2 adaptive, 3 mailbox
	int max_fps = 0; // 0 = unlimited
	real_t render_scale = 1.0; // >1 supersamples, <1 upscales
	int upscaler = 0; // 0 bilinear, 1 FSR1, 2 FSR2, 3 MetalFX spatial, 4 MetalFX temporal
	int antialiasing = 2; // 0 off, 1 MSAA2x, 2 MSAA4x, 3 MSAA8x, 4 FXAA, 5 TAA
	int texture_filter = 2; // 0 nearest, 1 linear, 2 anisotropic
	int lighting = 0; // 0 original, 1 enhanced
	real_t fov = 0; // 0 = original (70)
	int camera_mode = 0; // 0 chase, 1 overhead (camera_fromadistance)
	bool show_fps = false;
	// Gameplay
	real_t game_speed = 1.0;
	int physics_substeps = 2; // Jolt ticks per 12.5 ms of the original's simulation
	bool help_every_race = false; // the original showed help before every race
	// Audio
	real_t music_volume = 1.0;
	real_t effects_volume = 1.0;

	void load();
	void save() const;
};

} // namespace ssrc
