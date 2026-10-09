// Shared helpers for the Supersonic RC port.
//
// Game logic is written in the original's own space ("Director space"):
// Z up, 1 unit = 1 inch, exactly as the Lingo scripts did, so every constant
// (spring strength 6300, rest length 30, checkpoint radius 300...) carries
// over unchanged. Godot works in Y-up metres; we convert at the boundary.
#pragma once

#include <godot_cpp/classes/json.hpp>
#include <godot_cpp/classes/node.hpp>
#include <godot_cpp/variant/basis.hpp>
#include <godot_cpp/variant/dictionary.hpp>
#include <godot_cpp/variant/string.hpp>
#include <godot_cpp/variant/transform3d.hpp>
#include <godot_cpp/variant/vector3.hpp>

namespace ssrc {

using namespace godot;

constexpr real_t WORLD_SCALE = 0.0254; // metres per Director unit (inch)
constexpr real_t INV_WORLD_SCALE = 1.0 / WORLD_SCALE;

// Director (x, y, z) -> Godot (x, z, -y), no scaling (directions).
inline Vector3 d2g_dir(const Vector3 &d) { return Vector3(d.x, d.z, -d.y); }
inline Vector3 g2d_dir(const Vector3 &g) { return Vector3(g.x, -g.z, g.y); }
// Positions / lengths: also scale inches <-> metres.
inline Vector3 d2g(const Vector3 &d) { return d2g_dir(d) * WORLD_SCALE; }
inline Vector3 g2d(const Vector3 &g) { return g2d_dir(g) * INV_WORLD_SCALE; }

// Rotation between spaces: R_g = M R_d M^T, with M the axis permutation.
inline Basis axis_map() { return Basis(Vector3(1, 0, 0), Vector3(0, 0, -1), Vector3(0, 1, 0)); }
inline Basis d2g_basis(const Basis &d) {
	Basis m = axis_map();
	return m * d * m.transposed();
}
inline Basis g2d_basis(const Basis &g) {
	Basis m = axis_map();
	return m.transposed() * g * m;
}

// Director-space rigid transform (rotation + translation in inches).
struct DXform {
	Basis basis;
	Vector3 origin;
	Vector3 xform(const Vector3 &p) const { return basis.xform(p) + origin; }
	Vector3 inv_xform(const Vector3 &p) const { return basis.xform_inv(p - origin); }
	Vector3 x_axis() const { return basis.get_column(0); }
	Vector3 y_axis() const { return basis.get_column(1); }
	Vector3 z_axis() const { return basis.get_column(2); }
};

inline Transform3D dxform_to_godot(const DXform &d) { return Transform3D(d2g_basis(d.basis), d2g(d.origin)); }
inline DXform godot_to_dxform(const Transform3D &g) { return DXform{ g2d_basis(g.basis), g2d(g.origin) }; }

// Director's sign(): 0 counts as positive.
inline real_t lsign(real_t v) { return v >= 0 ? 1.0 : -1.0; }

// Rotation about Director-space Z by degrees (Lingo's vector(0,0,deg)).
inline Basis rot_z_deg(real_t deg) { return Basis(Vector3(0, 0, 1), Math::deg_to_rad(deg)); }

Variant load_json_file(const String &path);

// Director model/group names are case-insensitive; Godot may also re-case
// names on import, so always look nodes up this way.
godot::Node *find_child_ci(godot::Node *root, const String &name);

} // namespace ssrc
