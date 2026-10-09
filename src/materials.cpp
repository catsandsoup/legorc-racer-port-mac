#include "materials.h"
#include "assets.h"

#include <godot_cpp/classes/base_material3d.hpp>
#include <godot_cpp/classes/image.hpp>
#include <godot_cpp/classes/os.hpp>
#include <godot_cpp/classes/rendering_server.hpp>
#include <godot_cpp/classes/mesh.hpp>
#include <godot_cpp/classes/mesh_instance3d.hpp>
#include <godot_cpp/classes/shader.hpp>
#include <godot_cpp/classes/shader_material.hpp>
#include <godot_cpp/classes/standard_material3d.hpp>
#include <godot_cpp/templates/hash_map.hpp>

namespace ssrc {

String sanitize_name(const String &s) {
	String out;
	for (int i = 0; i < s.length(); i++) {
		char32_t c = s[i];
		bool ok = (c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') || (c >= '0' && c <= '9') || c == '-' || c == '_';
		out += ok ? String::chr(c) : String("_");
	}
	return out;
}

static const char *SHADER_BODY = R"(
uniform sampler2D albedo_tex : hint_default_white, FILTER, repeat_enable;
uniform sampler2D lightmap_tex : hint_default_white, filter_linear_mipmap, repeat_enable;
uniform bool has_albedo = false;
uniform bool has_lightmap = false;
uniform vec3 diffuse = vec3(1.0);
uniform vec3 ambient_mat = vec3(0.25);
uniform vec3 emissive = vec3(0.0);
uniform float opacity = 1.0;
uniform vec3 ambient_light = vec3(0.0);
uniform int light_count = 0;
uniform vec3 light_dir[8];
uniform vec3 light_col[8];
uniform vec2 uv_scale = vec2(1.0);
uniform int lm_mode = 0;
uniform vec2 uv_offset = vec2(0.0);
varying vec3 world_normal;

void vertex() {
	world_normal = normalize(MODEL_NORMAL_MATRIX * NORMAL);
	UV = UV * uv_scale + uv_offset;
}

vec3 srgb_to_linear(vec3 c) {
	return mix(pow((c + vec3(0.055)) / vec3(1.055), vec3(2.4)), c / vec3(12.92), lessThan(c, vec3(0.04045)));
}

void fragment() {
	vec3 n = normalize(world_normal);
	if (!FRONT_FACING) {
		n = -n;
	}
	vec3 lit = emissive + ambient_mat * ambient_light;
	for (int i = 0; i < light_count; i++) {
		lit += diffuse * light_col[i] * max(dot(n, -light_dir[i]), 0.0);
	}
	lit = clamp(lit, vec3(0.0), vec3(1.0));
	vec4 tex = has_albedo ? texture(albedo_tex, UV) : vec4(1.0);
	vec3 c = tex.rgb * lit;
	if (has_lightmap) {
		vec3 lm = texture(lightmap_tex, UV2).rgb;
		if (lm_mode == 0) c *= lm;
		else if (lm_mode == 1) c *= lm * 2.0;
		else if (lm_mode == 2) c = c + lm;
		else if (lm_mode == 3) c *= lm * 4.0;
		else if (lm_mode == 4) c = tex.rgb * clamp(lit + lm, 0.0, 1.0);
		c = clamp(c, vec3(0.0), vec3(1.0));
	}
	ALBEDO = TO_LINEAR(c);
	ALPHA_LINE
}
)";

static Ref<Shader> director_shader(bool cull_disabled, bool alpha, bool overlay, int filter) {
	static HashMap<int, Ref<Shader>> *cache = nullptr;
	if (!cache) {
		cache = new HashMap<int, Ref<Shader>>();
	}
	int key = (cull_disabled ? 1 : 0) | (alpha ? 2 : 0) | (overlay ? 4 : 0) | (filter << 3);
	if (cache->has(key)) {
		return (*cache)[key];
	}
	String modes = "unshaded, shadows_disabled, ";
	modes += cull_disabled ? "cull_disabled" : "cull_back";
	if (alpha) {
		modes += ", blend_mix, depth_prepass_alpha";
	}
	if (overlay) {
		modes += ", depth_test_disabled";
	}
	String body = SHADER_BODY;
	body = body.replace("FILTER", filter == 0 ? "filter_nearest_mipmap" : filter == 1 ? "filter_linear_mipmap" : "filter_linear_mipmap_anisotropic");
	body = body.replace("ALPHA_LINE", alpha ? "ALPHA = tex.a * opacity;" : "");
	// Forward+/Mobile convert linear ALBEDO to sRGB on output; the
	// Compatibility renderer writes it as is. Director worked in gamma space.
	bool linear_out = RenderingServer::get_singleton()->get_current_rendering_method() != "gl_compatibility";
	body = body.replace("TO_LINEAR(c)", linear_out ? "srgb_to_linear(c)" : "c");
	Ref<Shader> sh;
	sh.instantiate();
	sh->set_code(String("shader_type spatial;\nrender_mode ") + modes + ";\n" + body);
	(*cache)[key] = sh;
	return sh;
}

static Color arr_color(const Variant &v, const Color &def) {
	Array a = v;
	return a.size() >= 3 ? Color(a[0], a[1], a[2]) : def;
}

Ref<Material> make_material(const MaterialParams &p, const DirectorLights &lights) {
	const Dictionary &sh = p.shader;
	Color diffuse = arr_color(sh.get("diffuse", Variant()), Color(1, 1, 1));
	if (p.diffuse_override.r >= 0) {
		diffuse = p.diffuse_override;
	}
	Color ambient = arr_color(sh.get("ambient", Variant()), Color(0.25, 0.25, 0.25));
	Color emissive = arr_color(sh.get("emissive", Variant()), Color(0, 0, 0));
	real_t opacity = sh.get("opacity", 1.0);
	bool use_diffuse = bool(sh.get("use_diffuse_with_texture", false)) || p.force_diffuse;
	bool tex_alpha = false;
	if (p.albedo.is_valid()) {
		Ref<Texture2D> t = p.albedo;
		if (!t->has_meta("ssrc_alpha")) {
			Ref<Image> img = t->get_image();
			t->set_meta("ssrc_alpha", img.is_valid() && img->detect_alpha() != Image::ALPHA_NONE);
		}
		tex_alpha = bool(t->get_meta("ssrc_alpha"));
	}
	bool alpha = opacity < 0.999 || tex_alpha;
	Vector2 uv_scale(1, 1), uv_offset;
	Array layers = sh.get("layers", Array());
	if (layers.size() > 0) {
		Array tt = Dictionary(layers[0]).get("tex_transform", Array());
		if (tt.size() == 16) {
			uv_scale = Vector2(tt[0], tt[5]);
			uv_offset = Vector2(tt[12], tt[13]);
		}
	}
	// Director ignores the diffuse colour on textured surfaces unless
	// useDiffuseWithTexture is set.
	Color lit_diffuse = (p.albedo.is_valid() && !use_diffuse) ? Color(1, 1, 1) : diffuse;

	if (p.lighting == 0) {
		Ref<ShaderMaterial> m;
		m.instantiate();
		m->set_shader(director_shader(p.double_sided, alpha, p.overlay, p.texture_filter));
		m->set_shader_parameter("has_albedo", p.albedo.is_valid());
		if (p.albedo.is_valid()) {
			m->set_shader_parameter("albedo_tex", p.albedo);
		}
		m->set_shader_parameter("has_lightmap", p.lightmap.is_valid());
		if (p.lightmap.is_valid()) {
			m->set_shader_parameter("lightmap_tex", p.lightmap);
		}
		m->set_shader_parameter("diffuse", Vector3(lit_diffuse.r, lit_diffuse.g, lit_diffuse.b));
		m->set_shader_parameter("ambient_mat", Vector3(ambient.r, ambient.g, ambient.b));
		m->set_shader_parameter("emissive", Vector3(emissive.r, emissive.g, emissive.b));
		m->set_shader_parameter("opacity", opacity);
		m->set_shader_parameter("ambient_light", Vector3(lights.ambient.r, lights.ambient.g, lights.ambient.b));
		PackedVector3Array dirs, cols;
		for (size_t i = 0; i < lights.dirs_g.size() && i < 8; i++) {
			dirs.push_back(lights.dirs_g[i]);
			cols.push_back(Vector3(lights.colors[i].r, lights.colors[i].g, lights.colors[i].b));
		}
		while (dirs.size() < 8) {
			dirs.push_back(Vector3(0, -1, 0));
			cols.push_back(Vector3());
		}
		m->set_shader_parameter("light_count", int(MIN(lights.dirs_g.size(), size_t(8))));
		m->set_shader_parameter("light_dir", dirs);
		m->set_shader_parameter("light_col", cols);
		m->set_shader_parameter("uv_scale", uv_scale);
		m->set_shader_parameter("uv_offset", uv_offset);
		static int lm_mode = OS::get_singleton()->get_environment("SSRC_LM_MODE").to_int();
		m->set_shader_parameter("lm_mode", lm_mode);
		if (p.overlay) {
			m->set_render_priority(100);
		}
		return m;
	}

	Ref<StandardMaterial3D> m;
	m.instantiate();
	if (p.albedo.is_valid()) {
		m->set_texture(BaseMaterial3D::TEXTURE_ALBEDO, p.albedo);
	}
	Color albedo = lit_diffuse;
	albedo.a = opacity;
	m->set_albedo(albedo);
	if (emissive.r + emissive.g + emissive.b > 0.001) {
		m->set_feature(BaseMaterial3D::FEATURE_EMISSION, true);
		m->set_emission(emissive);
	}
	m->set_metallic(0);
	m->set_roughness(0.85);
	m->set_specular(0.3);
	if (alpha) {
		m->set_transparency(BaseMaterial3D::TRANSPARENCY_ALPHA_DEPTH_PRE_PASS);
	}
	if (p.double_sided) {
		m->set_cull_mode(BaseMaterial3D::CULL_DISABLED);
	}
	if (p.lightmap.is_valid()) {
		m->set_feature(BaseMaterial3D::FEATURE_DETAIL, true);
		m->set_texture(BaseMaterial3D::TEXTURE_DETAIL_ALBEDO, p.lightmap);
		m->set_detail_uv(BaseMaterial3D::DETAIL_UV_2);
		m->set_detail_blend_mode(BaseMaterial3D::BLEND_MODE_MUL);
	}
	m->set_uv1_scale(Vector3(uv_scale.x, uv_scale.y, 1));
	m->set_uv1_offset(Vector3(uv_offset.x, uv_offset.y, 0));
	m->set_texture_filter(p.texture_filter == 0 ? BaseMaterial3D::TEXTURE_FILTER_NEAREST_WITH_MIPMAPS
					: p.texture_filter == 1 ? BaseMaterial3D::TEXTURE_FILTER_LINEAR_WITH_MIPMAPS
											: BaseMaterial3D::TEXTURE_FILTER_LINEAR_WITH_MIPMAPS_ANISOTROPIC);
	if (p.overlay) {
		m->set_flag(BaseMaterial3D::FLAG_DISABLE_DEPTH_TEST, true);
		m->set_render_priority(100);
	}
	return m;
}

void apply_director_materials(Node *n, const String &tex_dir, const DirectorLights &lights, int lighting, int texture_filter, bool overlay) {
	MeshInstance3D *mi = Object::cast_to<MeshInstance3D>(n);
	if (mi && mi->get_mesh().is_valid()) {
		Dictionary extras = mi->has_meta("extras") ? Dictionary(mi->get_meta("extras")) : Dictionary();
		int vis = int(double(extras.get("visibility", 1)));
		if (vis == 0) {
			mi->set_visible(false);
		}
		String lightmap = extras.get("lightmap", "");
		Ref<Texture2D> lm_tex = lightmap.is_empty() ? Ref<Texture2D>() : Assets::texture("lightmaps/" + lightmap + "LightingMap.png");
		Ref<Mesh> mesh = mi->get_mesh();
		for (int s = 0; s < mesh->get_surface_count(); s++) {
			Ref<Material> src = mesh->surface_get_material(s);
			MaterialParams p;
			p.shader = src.is_valid() && src->has_meta("extras") ? Dictionary(src->get_meta("extras")) : Dictionary();
			p.lighting = lighting;
			p.texture_filter = texture_filter;
			p.overlay = overlay;
			String shader_name = p.shader.get("w3d_shader", "");
			Array layers = p.shader.get("layers", Array());
			String base_tex = layers.size() > 0 ? String(Dictionary(layers[0]).get("texture", "")) : String();
			if (!base_tex.is_empty() && base_tex != "DefaultTexture") {
				p.albedo = Assets::texture(String("scenes/") + tex_dir + "/" + sanitize_name(base_tex) + ".png");
			}
			Ref<BaseMaterial3D> src3d = src;
			p.double_sided = vis == 3 || (src3d.is_valid() && src3d->get_cull_mode() == BaseMaterial3D::CULL_DISABLED);
			// lightMapManager.new: each lightmapped model gets CLONES of its
			// shaders with the lightmap as texture layer 2 and
			// useDiffuseWithTexture = 1. The three shaders it then retunes by
			// name (column, Ex_Main_02, D_Clone) are the originals, so the
			// retune only reaches models that were not lightmapped.
			if (lm_tex.is_valid()) {
				p.lightmap = lm_tex;
				p.force_diffuse = true;
			} else if (shader_name == "column" || shader_name == "Ex_Main_02") {
				p.diffuse_override = Color::html("111133");
				p.force_diffuse = true;
				p.shader["ambient"] = Array({ 0, 0, 0 });
			} else if (shader_name == "D_Clone") {
				p.diffuse_override = Color::html("441111");
				p.shader["ambient"] = Array({ 0, 0, 0 });
			}
			mi->set_surface_override_material(s, make_material(p, lights));
		}
	}
	for (int i = 0; i < n->get_child_count(); i++) {
		apply_director_materials(n->get_child(i), tex_dir, lights, lighting, texture_filter, overlay);
	}
}

} // namespace ssrc
