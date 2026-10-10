#include "menu_ui.h"

#include <godot_cpp/classes/font_variation.hpp>
#include <godot_cpp/classes/image.hpp>
#include <godot_cpp/classes/image_texture.hpp>
#include <godot_cpp/classes/input_event_joypad_button.hpp>
#include <godot_cpp/classes/input_event_joypad_motion.hpp>
#include <godot_cpp/classes/input_event_key.hpp>
#include <godot_cpp/classes/input_event_mouse_button.hpp>
#include <godot_cpp/classes/input_event_mouse_motion.hpp>
#include <godot_cpp/classes/resource_loader.hpp>
#include <godot_cpp/classes/style_box_flat.hpp>
#include <godot_cpp/classes/texture_rect.hpp>
#include <godot_cpp/classes/theme_db.hpp>
#include <godot_cpp/core/math.hpp>

namespace ssrc {

Ref<Font> menu_font() {
	static Ref<FontVariation> *cached = nullptr;
	if (!cached) {
		cached = new Ref<FontVariation>();
		Ref<Font> base = ResourceLoader::get_singleton()->load("res://fonts/DejaVuSans-Bold.ttf");
		if (base.is_null()) {
			base = ThemeDB::get_singleton()->get_fallback_font();
		}
		cached->instantiate();
		(*cached)->set_base_font(base);
		(*cached)->set_variation_transform(Transform2D(Vector2(1, 0), Vector2(0.22, 1), Vector2()));
	}
	return *cached;
}

// Brushed metal in the original panel's greys (#a1a1a1..#aaaaaa), drawn
// procedurally so no game art is copied: horizontal streaks of slightly
// varying brightness.
Ref<Texture2D> brushed_metal() {
	static Ref<ImageTexture> *cached = nullptr;
	if (!cached) {
		cached = new Ref<ImageTexture>();
		const int W = 512, H = 256;
		Ref<Image> img = Image::create_empty(W, H, false, Image::FORMAT_RGB8);
		uint32_t seed = 12345;
		auto rnd = [&seed]() {
			seed = seed * 1664525u + 1013904223u;
			return (seed >> 8) / real_t(1 << 24);
		};
		for (int y = 0; y < H; y++) {
			real_t row = 0.640 + (rnd() - 0.5) * 0.035;
			real_t streak = 0;
			for (int x = 0; x < W; x++) {
				if (rnd() < 0.02) {
					streak = (rnd() - 0.5) * 0.05;
				}
				streak *= 0.985;
				real_t v = CLAMP(row + streak + (rnd() - 0.5) * 0.012, 0.0, 1.0);
				img->set_pixel(x, y, Color(v, v, v));
			}
		}
		*cached = ImageTexture::create_from_image(img);
	}
	return *cached;
}

static Ref<StyleBoxFlat> box(const Color &bg, const Color &border, int border_w, int radius) {
	Ref<StyleBoxFlat> s;
	s.instantiate();
	s->set_bg_color(bg);
	s->set_border_color(border);
	s->set_border_width_all(border_w);
	s->set_corner_radius_all(radius);
	s->set_anti_aliased(true);
	return s;
}

static Label *make_label(Control *parent, const String &text, int size, const Color &c) {
	Label *l = memnew(Label);
	l->set_text(text);
	l->add_theme_font_override("font", menu_font());
	l->add_theme_font_size_override("font_size", size);
	l->add_theme_color_override("font_color", c);
	l->set_mouse_filter(Control::MOUSE_FILTER_IGNORE);
	parent->add_child(l);
	return l;
}

MetalMenu::MetalMenu(Control *parent, const String &title, const Rect2 &p_rect, Layout p_layout) :
		layout(p_layout), rect(p_rect) {
	root = memnew(Control);
	root->set_position(rect.position);
	root->set_size(rect.size);
	root->set_mouse_filter(Control::MOUSE_FILTER_IGNORE);
	parent->add_child(root);

	// Rounded metal panel with a dark rim; the brushed texture is clipped to it.
	panel = memnew(Panel);
	panel->set_size(rect.size);
	Ref<StyleBoxFlat> frame = box(ui::METAL, ui::METAL_EDGE, 3, layout == BAR ? 12 : 18);
	frame->set_shadow_color(Color(0, 0, 0, 0.55));
	frame->set_shadow_size(layout == BAR ? 4 : 10);
	panel->add_theme_stylebox_override("panel", frame);
	panel->set_clip_children_mode(CanvasItem::CLIP_CHILDREN_AND_DRAW);
	panel->set_mouse_filter(Control::MOUSE_FILTER_IGNORE);
	root->add_child(panel);
	TextureRect *metal = memnew(TextureRect);
	metal->set_texture(brushed_metal());
	metal->set_expand_mode(TextureRect::EXPAND_IGNORE_SIZE);
	metal->set_stretch_mode(TextureRect::STRETCH_TILE);
	metal->set_position(Vector2(3, 3));
	metal->set_size(rect.size - Vector2(6, 6));
	metal->set_mouse_filter(Control::MOUSE_FILTER_IGNORE);
	panel->add_child(metal);

	if (!title.is_empty()) {
		// Chrome-style heading: light italic capitals with a heavy dark rim.
		Label *h = make_label(root, title.to_upper(), 30, ui::TEXT_LIGHT);
		h->add_theme_color_override("font_outline_color", Color(0.05, 0.05, 0.05));
		h->add_theme_constant_override("outline_size", 9);
		h->add_theme_color_override("font_shadow_color", Color(0, 0, 0, 0.5));
		h->add_theme_constant_override("shadow_offset_x", 2);
		h->add_theme_constant_override("shadow_offset_y", 3);
		h->set_horizontal_alignment(HORIZONTAL_ALIGNMENT_CENTER);
		h->set_position(Vector2(0, 8));
		h->set_size(Vector2(rect.size.x, 40));
	}
	if (layout != BAR) {
		prompts = make_label(root, "", 13, ui::INK);
		prompts->set_horizontal_alignment(HORIZONTAL_ALIGNMENT_CENTER);
		prompts->set_position(Vector2(0, rect.size.y - 26));
		prompts->set_size(Vector2(rect.size.x, 20));
	}
}

MetalMenu::~MetalMenu() {
	if (root) {
		root->queue_free();
	}
}

void MetalMenu::set_items(const std::vector<MenuItem> &p_items) {
	items = p_items;
	build_rows();
	set_focus(CLAMP(focus, 0, int(items.size()) - 1));
}

void MetalMenu::build_rows() {
	for (Panel *p : rows) {
		p->queue_free();
	}
	rows.clear();
	labels.clear();
	values.clear();
	int n = int(items.size());
	for (int i = 0; i < n; i++) {
		Panel *p = memnew(Panel);
		p->set_mouse_filter(Control::MOUSE_FILTER_IGNORE);
		Rect2 r;
		if (layout == BAR) {
			real_t w = (rect.size.x - 16 - 8 * (n - 1)) / n;
			r = Rect2(8 + i * (w + 8), 5, w, rect.size.y - 10);
		} else if (layout == LIST) {
			real_t h = 34, gap = 8, top = 58;
			r = Rect2(36, top + i * (h + gap), rect.size.x - 72, h);
		} else {
			real_t h = 25, gap = 3, top = 54;
			r = Rect2(20, top + i * (h + gap), rect.size.x - 40, h);
		}
		p->set_position(r.position);
		p->set_size(r.size);
		root->add_child(p);
		rows.push_back(p);
		int size = layout == SETTINGS ? 15 : (layout == BAR ? 16 : 19);
		Label *l = make_label(p, items[i].label, size, ui::INK);
		l->set_vertical_alignment(VERTICAL_ALIGNMENT_CENTER);
		l->set_size(r.size);
		if (layout == SETTINGS) {
			l->set_position(Vector2(14, 0));
			Label *v = make_label(p, "", size, ui::INK);
			v->set_horizontal_alignment(HORIZONTAL_ALIGNMENT_RIGHT);
			v->set_vertical_alignment(VERTICAL_ALIGNMENT_CENTER);
			v->set_position(Vector2(0, 0));
			v->set_size(r.size - Vector2(14, 0));
			values.push_back(v);
		} else {
			l->set_horizontal_alignment(HORIZONTAL_ALIGNMENT_CENTER);
			values.push_back(nullptr);
		}
		labels.push_back(l);
	}
	refresh();
}

void MetalMenu::style_row(int i) {
	bool f = i == focus;
	// The original's buttons: grey pills with dark labels; the focused one
	// lights up with a blue label and a pale glow (the "Over" art).
	Ref<StyleBoxFlat> s = box(f ? ui::PILL_FOCUS : ui::PILL, f ? ui::BLUE : Color(0.35, 0.35, 0.35), f ? 3 : 2, layout == SETTINGS ? 10 : 14);
	rows[i]->add_theme_stylebox_override("panel", s);
	for (Label *l : { labels[i], values[i] }) {
		if (!l) {
			continue;
		}
		l->add_theme_color_override("font_color", f ? ui::BLUE : ui::INK);
		l->add_theme_color_override("font_outline_color", ui::GLOW);
		l->add_theme_constant_override("outline_size", f ? 5 : 0);
	}
}

void MetalMenu::refresh() {
	for (int i = 0; i < int(items.size()) && i < int(rows.size()); i++) {
		if (values[i] && items[i].value) {
			String v = items[i].value();
			values[i]->set_text(i == focus ? String::utf8("◀  ") + v + String::utf8("  ▶") : v);
		}
		style_row(i);
	}
	if (prompts) {
		String sel, back;
		switch (device) {
			case InputDevice::XBOX: sel = "(A)"; back = "(B)"; break;
			case InputDevice::PLAYSTATION: sel = String::utf8("✕"); back = String::utf8("○"); break;
			default: sel = "Enter"; back = "Esc"; break;
		}
		String t = sel + " Select     " + back + " Back";
		if (layout == SETTINGS) {
			t = String::utf8("◀ ▶ Change     ") + t;
		}
		prompts->set_text(t);
	}
}

void MetalMenu::set_focus(int i) {
	if (items.empty()) {
		return;
	}
	focus = CLAMP(i, 0, int(items.size()) - 1);
	refresh();
}

void MetalMenu::set_device(InputDevice d) {
	if (d != device) {
		device = d;
		refresh();
	}
}

int MetalMenu::row_at(const Vector2 &p) const {
	for (int i = 0; i < int(rows.size()); i++) {
		if (Rect2(rows[i]->get_position(), rows[i]->get_size()).has_point(p)) {
			return i;
		}
	}
	return -1;
}

bool MetalMenu::handle_input(const Ref<InputEvent> &event) {
	if (items.empty()) {
		return false;
	}
	bool prev = layout == BAR ? event->is_action_pressed("menu_left", true) : event->is_action_pressed("menu_up", true);
	bool next = layout == BAR ? event->is_action_pressed("menu_right", true) : event->is_action_pressed("menu_down", true);
	if (prev) {
		set_focus((focus + int(items.size()) - 1) % int(items.size()));
		return true;
	}
	if (next) {
		set_focus((focus + 1) % int(items.size()));
		return true;
	}
	MenuItem &it = items[focus];
	if (layout == SETTINGS && it.change) {
		if (event->is_action_pressed("menu_left", true)) {
			it.change(-1);
			refresh();
			return true;
		}
		if (event->is_action_pressed("menu_right", true)) {
			it.change(1);
			refresh();
			return true;
		}
	}
	if (event->is_action_pressed("menu_accept")) {
		if (it.activate) {
			it.activate();
		} else if (it.change) {
			it.change(1);
			refresh();
		}
		return true;
	}
	if (event->is_action_pressed("menu_back")) {
		if (on_back) {
			on_back();
		}
		return true;
	}
	Ref<InputEventMouseMotion> mm = event;
	if (mm.is_valid()) {
		int r = row_at(root->get_local_mouse_position());
		if (r >= 0 && r != focus) {
			set_focus(r);
		}
		return false;
	}
	Ref<InputEventMouseButton> mb = event;
	if (mb.is_valid() && mb->is_pressed() && (mb->get_button_index() == MOUSE_BUTTON_LEFT || mb->get_button_index() == MOUSE_BUTTON_RIGHT)) {
		int r = row_at(root->get_local_mouse_position());
		if (r < 0) {
			return false;
		}
		set_focus(r);
		MenuItem &m = items[r];
		if (m.activate) {
			m.activate();
		} else if (m.change) {
			m.change(mb->get_button_index() == MOUSE_BUTTON_RIGHT ? -1 : 1);
			refresh();
		}
		return true;
	}
	return false;
}

} // namespace ssrc
