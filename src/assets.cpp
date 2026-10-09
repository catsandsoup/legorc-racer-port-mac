#include "assets.h"
#include "util.h"

#include <godot_cpp/classes/audio_stream_mp3.hpp>
#include <godot_cpp/classes/audio_stream_ogg_vorbis.hpp>
#include <godot_cpp/classes/audio_stream_wav.hpp>
#include <godot_cpp/classes/dir_access.hpp>
#include <godot_cpp/classes/file_access.hpp>
#include <godot_cpp/classes/gltf_document.hpp>
#include <godot_cpp/classes/gltf_state.hpp>
#include <godot_cpp/classes/image.hpp>
#include <godot_cpp/classes/image_texture.hpp>
#include <godot_cpp/classes/os.hpp>
#include <godot_cpp/variant/utility_functions.hpp>

namespace ssrc {

Assets::State &Assets::st() {
	static State *s = new State();
	return *s;
}

Variant load_json_file(const String &path) {
	if (!FileAccess::file_exists(path)) {
		return Variant();
	}
	String text = FileAccess::get_file_as_string(path);
	return JSON::parse_string(text);
}

Node *find_child_ci(Node *root, const String &name) {
	if (!root) {
		return nullptr;
	}
	for (int i = 0; i < root->get_child_count(); i++) {
		Node *c = root->get_child(i);
		if (String(c->get_name()).nocasecmp_to(name) == 0) {
			return c;
		}
		if (Node *r = find_child_ci(c, name)) {
			return r;
		}
	}
	return nullptr;
}

String Assets::user_root() {
	return OS::get_singleton()->get_user_data_dir();
}

String Assets::data_root() {
	String env = OS::get_singleton()->get_environment("SSRC_DATA");
	if (!env.is_empty()) {
		return env;
	}
	return user_root().path_join("data");
}

String Assets::mods_root() {
	return user_root().path_join("mods");
}

bool Assets::has_converted_data() {
	return FileAccess::file_exists(data_root().path_join("manifest.json")) &&
			FileAccess::file_exists(data_root().path_join("scenes/world.glb"));
}

void Assets::scan_mods() {
	st().mods.clear();
	String root = mods_root();
	DirAccess::make_dir_recursive_absolute(root);
	PackedStringArray dirs = DirAccess::get_directories_at(root);
	PackedStringArray order;
	String order_file = root.path_join("load_order.txt");
	if (FileAccess::file_exists(order_file)) {
		for (String line : FileAccess::get_file_as_string(order_file).split("\n")) {
			line = line.strip_edges();
			if (!line.is_empty() && !line.begins_with("#") && dirs.has(line)) {
				order.push_back(line);
			}
		}
	}
	dirs.sort();
	for (const String &d : dirs) {
		if (!order.has(d) && !d.begins_with(".") && !d.begins_with("_")) {
			order.push_back(d);
		}
	}
	for (const String &d : order) {
		st().mods.push_back(root.path_join(d));
	}
	clear_cache();
	if (!st().mods.is_empty()) {
		UtilityFunctions::print("[ssrc] mods: ", st().mods);
	}
}

PackedStringArray Assets::active_mods() {
	return st().mods;
}

String Assets::resolve(const String &rel) {
	for (int i = st().mods.size() - 1; i >= 0; i--) {
		String p = st().mods[i].path_join(rel);
		if (FileAccess::file_exists(p)) {
			return p;
		}
	}
	return data_root().path_join(rel);
}

bool Assets::exists(const String &rel) {
	return FileAccess::file_exists(resolve(rel));
}

void Assets::clear_cache() {
	st().texture_cache.clear();
	st().sound_cache.clear();
	st().meta_loaded = false;
}

void Assets::load_meta() {
	if (st().meta_loaded) {
		return;
	}
	st().meta_loaded = true;
	Variant img = json("images.json");
	st().images = img.get_type() == Variant::DICTIONARY ? Dictionary(img) : Dictionary();
	Variant snd = json("sounds.json");
	st().sounds = snd.get_type() == Variant::DICTIONARY ? Dictionary(snd) : Dictionary();
}

Variant Assets::json(const String &rel) {
	return load_json_file(resolve(rel));
}

Ref<Texture2D> Assets::texture(const String &rel) {
	String path = resolve(rel);
	if (st().texture_cache.has(path)) {
		return st().texture_cache[path];
	}
	Ref<Image> img = Image::load_from_file(path);
	Ref<Texture2D> tex;
	if (img.is_valid() && !img->is_empty()) {
		img->generate_mipmaps();
		tex = ImageTexture::create_from_image(img);
	} else {
		UtilityFunctions::push_warning("[ssrc] missing texture ", path);
	}
	st().texture_cache[path] = tex;
	return tex;
}

Dictionary Assets::image_info(const String &member_name) {
	load_meta();
	if (st().images.has(member_name)) {
		return st().images[member_name];
	}
	// Director member names are case-insensitive.
	Array keys = st().images.keys();
	for (int i = 0; i < keys.size(); i++) {
		if (String(keys[i]).nocasecmp_to(member_name) == 0) {
			return st().images[keys[i]];
		}
	}
	return Dictionary();
}

Ref<Texture2D> Assets::member_texture(const String &member_name) {
	Dictionary info = image_info(member_name);
	if (info.is_empty()) {
		return Ref<Texture2D>();
	}
	return texture(info["file"]);
}

Vector2 Assets::member_reg(const String &member_name) {
	Dictionary info = image_info(member_name);
	if (info.is_empty()) {
		return Vector2();
	}
	Array reg = info["reg"];
	return Vector2(reg[0], reg[1]);
}

Ref<AudioStream> Assets::sound(const String &member_name) {
	load_meta();
	String key = member_name.to_lower();
	if (st().sound_cache.has(key)) {
		return st().sound_cache[key];
	}
	String rel;
	Array keys = st().sounds.keys();
	for (int i = 0; i < keys.size(); i++) {
		if (String(keys[i]).to_lower() == key) {
			rel = Dictionary(st().sounds[keys[i]])["file"];
			break;
		}
	}
	Ref<AudioStream> stream;
	if (!rel.is_empty()) {
		// A mod may replace an .mp3 with .ogg or .wav of the same name.
		String base = rel.get_basename();
		String path;
		for (const char *ext : { ".ogg", ".wav", ".mp3" }) {
			String candidate = resolve(base + ext);
			if (FileAccess::file_exists(candidate)) {
				path = candidate;
				break;
			}
		}
		if (path.ends_with(".mp3")) {
			stream = AudioStreamMP3::load_from_file(path);
		} else if (path.ends_with(".ogg")) {
			stream = AudioStreamOggVorbis::load_from_file(path);
		} else if (path.ends_with(".wav")) {
			stream = AudioStreamWAV::load_from_file(path);
		}
	}
	if (stream.is_null()) {
		UtilityFunctions::push_warning("[ssrc] missing sound ", member_name);
	}
	st().sound_cache[key] = stream;
	return stream;
}

Node3D *Assets::scene(const String &rel) {
	String path = resolve(rel);
	Ref<GLTFDocument> doc;
	doc.instantiate();
	Ref<GLTFState> state;
	state.instantiate();
	Error err = doc->append_from_file(path, state);
	if (err != OK) {
		UtilityFunctions::push_error("[ssrc] cannot load scene ", path, " err=", err);
		return nullptr;
	}
	return Object::cast_to<Node3D>(doc->generate_scene(state));
}

} // namespace ssrc
