#include "car.h"
#include "assets.h"
#include "driving.h"
#include "game.h"
#include "materials.h"
#include "sound.h"

#include <godot_cpp/classes/collision_shape3d.hpp>
#include <godot_cpp/classes/convex_polygon_shape3d.hpp>
#include <godot_cpp/classes/physics_direct_body_state3d.hpp>
#include <godot_cpp/classes/physics_material.hpp>
#include <godot_cpp/classes/physics_server3d.hpp>
#include <godot_cpp/classes/quad_mesh.hpp>
#include <godot_cpp/classes/standard_material3d.hpp>
#include <godot_cpp/classes/random_number_generator.hpp>
#include <godot_cpp/variant/utility_functions.hpp>

namespace ssrc {

static int lingo_random(int n) {
	// Lingo random(n): 1..n
	return int(UtilityFunctions::randi_range(1, n));
}

// ---------------------------------------------------------------- Car

const Tuning &Car::tuning() const {
	return session->tuning();
}

Car::Car(DrivingSession *p_session, Node3D *parent) :
		session(p_session) {
	const Tuning &t = tuning();
	mass = t.car_mass;
	gravity_length = 9.81 * INV_WORLD_SCALE * t.gravity_factor;

	// --- rigid body (makeMovableRigidBody("car", 3, convex)) ---
	body = memnew(RigidBody3D);
	body->set_name("car");
	parent->add_child(body);

	Dictionary phys = Assets::json("physics/collision.json");
	Dictionary car_info = phys.get("car", Dictionary());
	Array pts = car_info.get("hull_points_local_director", Array());
	PackedVector3Array hull;
	AABB box;
	for (int i = 0; i < pts.size(); i++) {
		Array p = pts[i];
		Vector3 v(p[0], p[1], p[2]);
		box = i == 0 ? AABB(v, Vector3()) : box.expand(v);
		hull.push_back(d2g(v));
	}
	if (hull.is_empty()) {
		// Fallback: the 15 x 30 x 10 inch box of the original.
		box = AABB(Vector3(-7.5, -15, 0), Vector3(15, 30, 10));
		for (int i = 0; i < 8; i++) {
			hull.push_back(d2g(box.get_endpoint(i)));
		}
	}
	Ref<ConvexPolygonShape3D> shape;
	shape.instantiate();
	shape->set_points(hull);
	CollisionShape3D *cs = memnew(CollisionShape3D);
	cs->set_shape(shape);
	body->add_child(cs);

	// Havok puts the COM at the hull centre; the script then shifts it down.
	center_of_mass_local = box.get_center() + Vector3(0, 0, t.com_shift_z - t.lift_body);
	body->set_mass(mass);
	body->set_center_of_mass_mode(RigidBody3D::CENTER_OF_MASS_MODE_CUSTOM);
	body->set_center_of_mass(d2g(center_of_mass_local));
	// Box inertia of the hull (Director x,y,z -> Godot x,z,y).
	Vector3 e = box.size * WORLD_SCALE;
	real_t ixx = mass / 12.0 * (e.y * e.y + e.z * e.z);
	real_t iyy = mass / 12.0 * (e.x * e.x + e.z * e.z);
	real_t izz = mass / 12.0 * (e.x * e.x + e.y * e.y);
	body->set_inertia(Vector3(ixx, izz, iyy));
	Ref<PhysicsMaterial> mat;
	mat.instantiate();
	mat->set_friction(t.car_friction);
	mat->set_bounce(t.car_restitution);
	body->set_physics_material_override(mat);
	body->set_linear_damp_mode(RigidBody3D::DAMP_MODE_REPLACE);
	body->set_angular_damp_mode(RigidBody3D::DAMP_MODE_REPLACE);
	body->set_linear_damp(0);
	body->set_angular_damp(t.angular_damp);
	body->set_gravity_scale(t.gravity_factor);
	body->set_can_sleep(false);
	body->set_use_continuous_collision_detection(true);
	body->set_contact_monitor(true);
	body->set_max_contacts_reported(8);
	body->set_collision_layer(DrivingSession::LAYER_CAR);
	body->set_collision_mask(DrivingSession::LAYER_WORLD);

	xform.basis = Basis();
	xform.origin = Vector3(t.start_x, t.start_y, t.start_z);
	body->set_global_transform(dxform_to_godot(xform));

	// --- visuals: holypolycrap cloned as "car2" under the physics model ---
	director_root = memnew(Node3D);
	director_root->set_name("director_space");
	director_root->set_transform(Transform3D(axis_map().scaled(Vector3(WORLD_SCALE, WORLD_SCALE, WORLD_SCALE)), Vector3()));
	body->add_child(director_root);

	Node3D *car2 = memnew(Node3D);
	car2->set_name("car2");
	Basis b2 = rot_z_deg(180).scaled(Vector3(0.17, 0.17, 0.17));
	car2->set_transform(Transform3D(b2, Vector3(0, 0, 4 - t.lift_body)));
	director_root->add_child(car2);

	Node3D *scene = Assets::scene("scenes/car.glb");
	Node3D *car_model = Object::cast_to<Node3D>(find_child_ci(scene, "Car"));
	std::vector<Node3D *> wheel_models;
	if (car_model) {
		const Settings &st = session->get_game()->settings();
		apply_director_materials(car_model, "car_textures", session->get_lights(), st.lighting, st.texture_filter);
		car_model->get_parent()->remove_child(car_model);
		car_model->set_transform(Transform3D());
		car2->add_child(car_model);
		for (int i = 0; i < car_model->get_child_count(); i++) {
			if (Node3D *w = Object::cast_to<Node3D>(car_model->get_child(i))) {
				wheel_models.push_back(w);
			}
		}
	}
	if (scene) {
		scene->queue_free();
	}

	// --- wheels: (-x,+y) FL, (+x,+y) FR traction/steer; rear free ---
	Vector3 wp[4] = {
		Vector3(-t.wheel_x, t.wheel_y, t.wheel_z - t.lift_body),
		Vector3(t.wheel_x, t.wheel_y, t.wheel_z - t.lift_body),
		Vector3(-t.wheel_x, -t.wheel_y, t.wheel_z - t.lift_body),
		Vector3(t.wheel_x, -t.wheel_y, t.wheel_z - t.lift_body),
	};
	for (int i = 0; i < 4; i++) {
		Node3D *vis = i < int(wheel_models.size()) ? wheel_models[i] : nullptr;
		Vector3 vscale(0.17, 0.17, 0.17);
		if (vis) {
			vscale *= vis->get_transform().basis.get_scale();
			vis->get_parent()->remove_child(vis);
			session->get_world_root()->add_child(vis);
		}
		wheels.push_back(new Wheel(this, wp[i], i < 2, i + 1, vis, vscale));
	}

	// --- blob shadow (carshadow, 50% blend, unlit) ---
	Ref<QuadMesh> quad;
	quad.instantiate();
	quad->set_size(Vector2(32, 78) * WORLD_SCALE);
	Ref<StandardMaterial3D> sm;
	sm.instantiate();
	sm->set_shading_mode(BaseMaterial3D::SHADING_MODE_UNSHADED);
	sm->set_transparency(BaseMaterial3D::TRANSPARENCY_ALPHA);
	sm->set_texture(BaseMaterial3D::TEXTURE_ALBEDO, Assets::member_texture("carshadow"));
	sm->set_albedo(Color(1, 1, 1, 0.5));
	sm->set_cull_mode(BaseMaterial3D::CULL_DISABLED);
	shadow = memnew(MeshInstance3D);
	shadow->set_name("carshadow");
	shadow->set_mesh(quad);
	shadow->set_material_override(sm);
	shadow->set_cast_shadows_setting(GeometryInstance3D::SHADOW_CASTING_SETTING_OFF);
	session->get_world_root()->add_child(shadow);

	for (int i = 1; i <= 3; i++) {
		crash_down.push_back(String("crash") + String::num_int64(i) + "_glass");
	}
	for (int i = 1; i <= 8; i++) {
		crash_up.push_back(String("crash") + String::num_int64(i));
	}

	// carsound.new
	SoundSystem *snd = session->sound();
	snd->set_volume(1, 25);
	snd->set_volume(2, 20);
	snd->stop(8);
	snd->play(8, PlayItem{ String("musicloop") + String::num_int64(lingo_random(5)) });
	snd->fade_to(8, 75, 500);

	read_state();
}

void Car::shutdown_sound() {
	// carsound.destroy
	SoundSystem *snd = session->sound();
	snd->stop(1);
	snd->stop(2);
}

Car::~Car() {
	for (Wheel *w : wheels) {
		delete w;
	}
	wheels.clear();
}

void Car::read_state() {
	Transform3D g = body->get_global_transform();
	xform = godot_to_dxform(g);
	xform.basis.orthonormalize();
	linear_velocity = g2d(body->get_linear_velocity());
	angular_velocity = g2d_dir(body->get_angular_velocity());
}

void Car::apply_force_at_point(const Vector3 &force_d, const Vector3 &point_local_d) {
	// Havok's applyForceAtPoint: linear force through the COM plus r x F,
	// both attenuated the way the Xtra's integrator did (see Tuning).
	const Tuning &t = tuning();
	Vector3 r = xform.basis.xform(point_local_d - center_of_mass_local);
	Vector3 torque = r.cross(force_d);
	torque.x /= t.torque_divider_pitch_roll;
	torque.y /= t.torque_divider_pitch_roll;
	torque.z /= t.torque_divider_yaw;
	forces.push_back(PendingForce{ d2g(force_d / t.force_divider), d2g_dir(torque) * (WORLD_SCALE * WORLD_SCALE) });
}

void Car::flush_forces() {
	for (const PendingForce &f : forces) {
		body->apply_central_force(f.force_g);
		body->apply_torque(f.torque_g);
	}
}

void Car::update(const DriveInput &in) {
	update_sound(in);
	for (Wheel *w : wheels) {
		w->update(stopped ? DriveInput() : in);
	}
}

void Car::after_physics_contacts() {
	// 'collision' callback: limit spin while touching anything, and crash sounds.
	PhysicsDirectBodyState3D *st = PhysicsServer3D::get_singleton()->body_get_direct_state(body->get_rid());
	if (!st || st->get_contact_count() == 0) {
		return;
	}
	const real_t max_w = tuning().max_angular_on_collision;
	Vector3 w = body->get_angular_velocity();
	if (w.length() > max_w) {
		body->set_angular_velocity(w.normalized() * max_w);
	}
	real_t impact = 0;
	for (int i = 0; i < st->get_contact_count(); i++) {
		Vector3 n = st->get_contact_local_normal(i);
		Vector3 rel = st->get_contact_local_velocity_at_position(i) - st->get_contact_collider_velocity_at_position(i);
		impact = MAX(impact, Math::abs(rel.dot(n)) * INV_WORLD_SCALE);
	}
	SoundSystem *snd = session->sound();
	if (impact > 1.0 && !snd->is_busy(3)) {
		std::vector<String> &list = xform.z_axis().z > 0.5 ? crash_up : crash_down;
		String m = list.front();
		list.erase(list.begin());
		list.push_back(m);
		snd->play(3, PlayItem{ m });
		snd->set_volume(3, 125);
	}
}

void Car::step_frame() {
	RayHit hit = session->raycast(xform.origin, Vector3(0, 0, -1), 5000.0, true);
	if (!hit.hit) {
		shadow->set_visible(false);
		return;
	}
	shadow->set_visible(true);
	Vector3 n = hit.normal.normalized();
	Vector3 fwd = xform.y_axis();
	fwd = (fwd - n * fwd.dot(n));
	if (fwd.length_squared() < 1e-6) {
		fwd = Vector3(0, 1, 0);
	}
	fwd.normalize();
	Vector3 x = fwd.cross(n).normalized();
	Vector3 pos = hit.position + n * 2.0;
	// QuadMesh lies in its local XY plane facing +Z; map: x->side, y->forward, z->normal.
	Basis gb(d2g_dir(x), d2g_dir(fwd), d2g_dir(n));
	shadow->set_global_transform(Transform3D(gb, d2g(pos)));
}

void Car::stop_driving() {
	sound_stopped = true;
	session->sound()->fade_out(8, 500);
	for (Wheel *w : wheels) {
		w->stop_driving();
	}
	stopped = true;
}

void Car::reset_to(const Vector3 &pos_d, bool reset_rotation) {
	Transform3D g = body->get_global_transform();
	g.origin = d2g(pos_d);
	if (reset_rotation) {
		g.basis = Basis();
	}
	body->set_global_transform(g);
	body->reset_physics_interpolation();
	read_state();
}

void Car::teleport_up(real_t dz) {
	reset_to(xform.origin + Vector3(0, 0, dz), true);
}

void Car::update_sound(const DriveInput &in) {
	SoundSystem *snd = session->sound();
	if (snd->playlist_count(8) < 3 && !sound_stopped) {
		snd->queue(8, PlayItem{ String("musicloop") + String::num_int64(lingo_random(5)) });
	}
	bool t126 = !sound_stopped && (in.up || in.down);
	bool t123 = !sound_stopped && (in.left || in.steer_axis < -0.3);
	bool t124 = !sound_stopped && (in.right || in.steer_axis > 0.3);
	if (t126 && !sound_playing) {
		real_t start = linear_velocity.length() * 4 - 150;
		snd->play(1, PlayItem{ "CarStart", 1, sound_shift, real_t(MAX(0.0, start)) });
		snd->queue(1, PlayItem{ "CarLoop", 0, sound_shift, 0 });
		sound_playing = true;
	} else if (sound_playing && !t126) {
		if (snd->current_member(1).to_lower() == "carstart") {
			real_t st = SoundSystem::member_duration_ms("CarEnd") - snd->current_time_ms(1) / 2 - 10;
			snd->play(1, PlayItem{ "CarEnd", 1, 0, real_t(MAX(0.0, st)) });
		} else {
			snd->play(1, PlayItem{ "CarEnd" });
		}
		sound_playing = false;
	}
	if (t123 && turn_state != -1) {
		snd->play(2, PlayItem{ String("CarTurnLeft0") + String::num_int64(lingo_random(3)) });
		turn_state = -1;
	} else if (t124 && turn_state != 1) {
		snd->play(2, PlayItem{ String("CarTurnRight0") + String::num_int64(lingo_random(3)) });
		turn_state = 1;
	}
	if (!t123 && !t124 && turn_state != 0) {
		snd->play(2, PlayItem{ String(turn_state == -1 ? "CarTurnRight0" : "CarTurnLeft0") + String::num_int64(lingo_random(3)) });
		turn_state = 0;
	}
	real_t shift = 2 - (int(wheels[0]->on_ground) + int(wheels[1]->on_ground)) * 0.5 + 0.1;
	if (reversing) {
		shift = 0.2;
	}
	if (!Math::is_equal_approx(sound_shift, shift) && sound_playing) {
		snd->play(1, PlayItem{ "CarLoop", 0, shift, 0 });
	}
	sound_shift = shift;
}

// ---------------------------------------------------------------- Wheel

Wheel::Wheel(Car *p_car, const Vector3 &local_pos, bool p_traction, int p_index, Node3D *p_visual, const Vector3 &p_visual_scale) :
		index(p_index), car(p_car), position(local_pos), traction(p_traction), visual_model(p_visual), visual_scale(p_visual_scale) {
	left = local_pos.x < 0;
	spring_length = car->tuning().rest_length;
	visual_group_pos = car->xform.xform(position);
}

DXform Wheel::world_xform() const {
	DXform local{ rot_z_deg(rotation), position };
	DXform w;
	w.basis = car->xform.basis * local.basis;
	w.origin = car->xform.xform(position);
	return w;
}

void Wheel::find_ray_isect() {
	const Tuning &t = car->tuning();
	DXform w = world_xform();
	Vector3 up = w.z_axis().normalized();
	Vector3 down = -up;
	ray_start = w.origin + up * t.ray_start_offset;
	RayHit hit = car->get_session()->raycast(ray_start, down, t.rest_length + 1.0, false);
	if (hit.hit) {
		ray_isect = hit.position;
		real_t d = ray_isect.distance_to(ray_start);
		if (d <= t.rest_length) {
			spring_length = MAX(t.min_spring_length, d);
			ground_normal = hit.normal;
			visual_group_pos = ray_start + down * spring_length + Vector3(0, 0, t.wheel_visual_drop);
			on_ground = true;
			return;
		}
	}
	on_ground = false;
	spring_length = t.rest_length;
	visual_group_pos = ray_start + down * (spring_length - t.wheel_visual_drop);
}

void Wheel::act_as_spring() {
	const Tuning &t = car->tuning();
	load = 0;
	if (!on_ground) {
		return;
	}
	real_t strength = car->mass * car->gravity_length / 4 * t.spring_factor;
	Vector3 diff = ray_isect - ray_start;
	real_t force_length = strength * (t.rest_length - spring_length);
	real_t speed_dir = speed.dot(diff.normalized());
	speed_dir = CLAMP(speed_dir, -t.damp_speed_limit, t.damp_speed_limit);
	real_t damping = strength * t.damp_factor * speed_dir;
	load = force_length + damping;
	Vector3 force = ground_normal.normalized() * load;
	car->apply_force_at_point(force, position);
}

void Wheel::update(const DriveInput &in) {
	const Tuning &t = car->tuning();
	bool t126 = in.up, t125 = in.down, t123 = in.left, t124 = in.right;
	if (stopped) {
		t126 = t125 = t123 = t124 = false;
	}
	find_ray_isect();
	DXform w = world_xform();
	speed = car->linear_velocity + car->angular_velocity.cross(w.origin - (car->xform.origin + car->center_of_mass_local));
	Vector3 flat = speed;
	flat.z = 0;
	if (on_ground) {
		distance_covered += flat.length() * 0.1;
	}
	Vector3 down = -w.z_axis().normalized();
	Vector3 real_world_pos = down * (spring_length - t.wheel_radius) + w.origin;
	act_as_spring();

	Vector3 side = ground_normal.cross(w.y_axis()).normalized();
	Vector3 forward = (-ground_normal.cross(side)).normalized();
	if (left) {
		side = -side;
	}
	real_t local_forward_speed = speed.dot(forward);
	real_t longitudinal = 0;
	if (t126) {
		longitudinal = t.drive_force;
	}
	car->reversing = false;
	if (t125) {
		if (local_forward_speed < 0) {
			longitudinal = -t.reverse_force;
			car->reversing = true;
		} else {
			longitudinal = -t.brake_force;
		}
	}
	bool handbrake = !stopped && in.handbrake;
	if ((handbrake && Math::abs(local_forward_speed) > t.handbrake_min_speed) || stopped) {
		longitudinal = -t.brake_force * lsign(local_forward_speed);
	}
	Vector3 wheel_pos = car->xform.inv_xform(real_world_pos);
	Vector3 total;
	if (traction && on_ground) {
		if (car->skidding) {
			longitudinal *= t.skid_drive_factor;
		}
		total += forward * longitudinal;
	}
	if (on_ground) {
		real_t drag = local_forward_speed * t.rolling_drag;
		if (car->skidding) {
			drag *= t.skid_drag_factor;
		}
		total -= forward * drag;
	}
	if (traction) {
		if (in.steer_axis != 0 && !stopped) {
			// Analog: step toward the stick target at the original rate.
			real_t target = -in.steer_axis * t.steer_max_deg;
			if (rotation < target) {
				rotation = MIN(target, rotation + t.steer_step_deg);
			} else if (rotation > target) {
				rotation = MAX(target, rotation - t.steer_step_deg);
			}
		} else if (t123) {
			rotation = MIN(t.steer_max_deg, rotation + t.steer_step_deg);
		} else if (t124) {
			rotation = MAX(-t.steer_max_deg, rotation - t.steer_step_deg);
		} else if (Math::abs(rotation) > 0) {
			rotation = rotation - lsign(rotation) * MIN(t.steer_step_deg, Math::abs(rotation));
		}
	}
	if (on_ground) {
		real_t side_speed = speed.dot(side);
		real_t side_drag = side_speed * t.side_drag;
		real_t max_drag = t.side_grip_per_load * load + (traction ? 1 : 0) * t.side_grip_traction_bonus;
		if (car->skidding) {
			max_drag *= t.skid_grip_factor;
		}
		if (handbrake && !traction) {
			max_drag *= t.handbrake_rear_grip;
		}
		if (Math::abs(side_drag) > max_drag) {
			side_drag *= 0.5;
			car->skidding = true;
		} else {
			car->skidding = false;
		}
		total -= side * side_drag;
	}
	car->apply_force_at_point(total, wheel_pos);
	sync_visual();
}

void Wheel::sync_visual() {
	if (!visual_model) {
		return;
	}
	const Tuning &t = car->tuning();
	Basis steer = traction ? rot_z_deg(-rotation * t.visual_steer_factor) : Basis();
	Basis spin(Vector3(1, 0, 0), Math::deg_to_rad(distance_covered));
	Basis flip = (index == 1 || index == 2) ? Basis(Vector3(0, 1, 0), Math_PI) : Basis();
	Basis bd = car->xform.basis * Basis().scaled(visual_scale) * flip * steer * spin;
	Basis bg = (axis_map() * bd).scaled(Vector3(WORLD_SCALE, WORLD_SCALE, WORLD_SCALE));
	visual_model->set_global_transform(Transform3D(bg, d2g(visual_group_pos)));
}

} // namespace ssrc
