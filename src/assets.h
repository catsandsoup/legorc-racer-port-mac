// Asset lookup with mod overrides.
//
// Converted game data lives in <user data>/data (written by ssrc-import on
// first launch). Mods live in <user data>/mods/<name>/ and mirror the same
// relative layout: dropping mods/shiny/scenes/car.glb swaps the car model,
// mods/hd/scenes/world_textures/Floor.png swaps one texture, and
// mods/x/tuning.json overrides physics constants. Later mods (alphabetical,
// or the order in mods/load_order.txt) win.
#pragma once

#include <godot_cpp/classes/audio_stream.hpp>
#include <godot_cpp/classes/node3d.hpp>
#include <godot_cpp/classes/texture2d.hpp>
#include <godot_cpp/templates/hash_map.hpp>
#include <godot_cpp/variant/packed_string_array.hpp>

namespace ssrc {

using namespace godot;

class Assets {
public:
	static String user_root();
	static String data_root();
	static String mods_root();
	static bool has_converted_data();

	static void scan_mods();
	static PackedStringArray active_mods();

	// Resolve a relative asset path through mods, then base data.
	static String resolve(const String &rel);
	static bool exists(const String &rel);

	static Ref<Texture2D> texture(const String &rel);
	static Ref<AudioStream> sound(const String &member_name);
	static Node3D *scene(const String &rel);
	static Variant json(const String &rel);

	// Image metadata from images.json (file, reg point, size).
	static Dictionary image_info(const String &member_name);
	static Ref<Texture2D> member_texture(const String &member_name);
	static Vector2 member_reg(const String &member_name);

	static void clear_cache();

private:
	// Godot types must not be constructed at library load time, so all
	// state lives in a lazily created object.
	struct State {
		PackedStringArray mods;
		HashMap<String, Ref<Texture2D>> texture_cache;
		HashMap<String, Ref<AudioStream>> sound_cache;
		Dictionary images;
		Dictionary sounds;
		bool meta_loaded = false;
	};
	static State &st();
	static void load_meta();
};

} // namespace ssrc
