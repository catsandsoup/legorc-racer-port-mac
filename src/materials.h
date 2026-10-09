// Materials for converted W3D shaders.
//
// "Original" lighting reproduces Director 8.5's fixed-function model in gamma
// space: texture x clamp(emissive + ambient + sum(diffuse * light * N.L)) x
// lightmap, with the scene's own directional and ambient lights.
// "Enhanced" lighting uses Godot's PBR lights (shadows, SSAO) instead.
#pragma once

#include <godot_cpp/classes/material.hpp>
#include <godot_cpp/classes/node.hpp>
#include <godot_cpp/classes/texture2d.hpp>
#include <godot_cpp/variant/color.hpp>
#include <godot_cpp/variant/dictionary.hpp>
#include <vector>

namespace ssrc {

using namespace godot;

struct DirectorLights {
	std::vector<Vector3> dirs_g; // direction light travels, Godot world space
	std::vector<Color> colors;
	Color ambient = Color(0, 0, 0);
};

struct MaterialParams {
	Dictionary shader; // W3D shader extras from the converter
	Ref<Texture2D> albedo;
	Ref<Texture2D> lightmap;
	bool double_sided = false;
	bool overlay = false; // drawn on top (the checkpoint arrow)
	bool force_diffuse = false; // lightMapManager: useDiffuseWithTexture = 1
	Color diffuse_override = Color(-1, -1, -1);
	int lighting = 0; // 0 original, 1 enhanced
	int texture_filter = 2;
};

Ref<Material> make_material(const MaterialParams &p, const DirectorLights &lights);

// Rebuild every surface material under `root` from its W3D extras.
// `tex_dir` is the converted texture folder (e.g. "world_textures").
void apply_director_materials(Node *root, const String &tex_dir, const DirectorLights &lights, int lighting, int texture_filter, bool overlay = false);

String sanitize_name(const String &s);

} // namespace ssrc
