// The RC car: a port of the 'car', 'wheel' and 'carsound' Lingo scripts.
//
// One rigid body (Jolt, via Godot's RigidBody3D) carries the 3 kg chassis
// box; four ray-cast wheels push it with spring, drive, drag and side-grip
// forces computed in Director space exactly as the original did.
#pragma once

#include "tuning.h"
#include "util.h"

#include <godot_cpp/classes/mesh_instance3d.hpp>
#include <godot_cpp/classes/node3d.hpp>
#include <godot_cpp/classes/rigid_body3d.hpp>
#include <vector>

namespace ssrc {

class SoundSystem;
class DrivingSession;

struct DriveInput {
	bool up = false; // keyPressed(126)
	bool down = false; // keyPressed(125)
	bool left = false; // keyPressed(123)
	bool right = false; // keyPressed(124)
	bool handbrake = false; // keyPressed(" ")
	real_t steer_axis = 0; // gamepad: -1 left .. 1 right (0 = use keys)
};

struct RayHit {
	bool hit = false;
	Vector3 position; // Director space
	Vector3 normal; // Director space
};

class Car;

class Wheel {
public:
	Wheel(Car *car, const Vector3 &local_pos, bool traction, int index, Node3D *visual_model, const Vector3 &visual_scale);
	void update(const DriveInput &in);
	void stop_driving() { stopped = true; }
	void sync_visual();

	bool on_ground = false;
	real_t load = 0;
	int index;

private:
	Car *car;
	Vector3 position; // pPosition / pmodel.transform.position (car-local)
	bool traction;
	bool left;
	bool stopped = false;
	real_t rotation = 0; // steering, degrees
	real_t steer_accum = 0; // for analog steering
	real_t spring_length;
	real_t distance_covered = 0;
	Vector3 ground_normal = Vector3(0, 0, 1);
	Vector3 ray_isect, ray_start;
	Vector3 speed;
	// Visuals (pVisualGroup / flip group / pVisualModel).
	Node3D *visual_group = nullptr;
	Node3D *visual_model = nullptr;
	Vector3 visual_scale;
	Vector3 visual_group_pos; // Director space
	DXform world_xform() const; // pmodel world transform

	void find_ray_isect();
	void act_as_spring();
};

class Car {
public:
	Car(DrivingSession *session, Node3D *parent);
	~Car();

	// Per logic tick (drivingManager.update -> car.update), Director space.
	void read_state();
	void update(const DriveInput &in);
	void step_frame(); // shadow, per tick
	void after_physics_contacts();
	void stop_driving();
	void shutdown_sound();
	void spawn_at(const DXform &x); // test hook: place at rest
	void reset_to(const Vector3 &pos_d, bool reset_rotation);
	void teleport_up(real_t dz);

	// Rigid body state in Director space.
	DXform xform; // body origin == model 'car' transform
	Vector3 linear_velocity;
	Vector3 angular_velocity;
	Vector3 center_of_mass_local; // Havok rb.centerOfMass (body-local)
	real_t mass;
	real_t gravity_length; // |pHavok.gravity| in Director units

	bool skidding = false;
	bool reversing = false;
	bool stopped = false;
	std::vector<Wheel *> wheels;

	void apply_force_at_point(const Vector3 &force_d, const Vector3 &point_local_d);
	// Forces are computed once per logic tick and applied on every physics
	// substep until the next tick (Havok applied queued forces to all 7
	// substeps of its step the same way).
	void clear_forces() { forces.clear(); }
	void flush_forces();
	RigidBody3D *get_body() const { return body; }
	Node3D *get_model_root() const { return director_root; }
	DrivingSession *get_session() const { return session; }
	const Tuning &tuning() const;

private:
	DrivingSession *session;
	RigidBody3D *body = nullptr;
	Node3D *director_root = nullptr; // Director-local frame under the body
	MeshInstance3D *shadow = nullptr;
	struct PendingForce {
		Vector3 force_g, torque_g;
	};
	std::vector<PendingForce> forces;
	std::vector<String> crash_up, crash_down;

	// carsound
	bool sound_playing = false;
	real_t sound_shift = 1;
	int turn_state = 0; // -1 left, 0 middle, 1 right
	bool sound_stopped = false;
	void update_sound(const DriveInput &in);
};

} // namespace ssrc
