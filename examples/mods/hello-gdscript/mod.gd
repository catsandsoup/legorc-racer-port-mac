# Example script mod: prints the car's speed once a second and makes the
# chase camera a little wider. Drop this folder into the Mods folder.
extends Node

var game: Node
var t := 0.0

func _ready() -> void:
	game = get_meta("game")
	game.session_started.connect(_on_session_started)

func _on_session_started(session: Node3D) -> void:
	var cam := session.get_node("camera") as Camera3D
	cam.fov = 80.0

func _process(delta: float) -> void:
	t += delta
	var session = game.get_session()
	if session == null or t < 1.0:
		return
	t = 0.0
	var car := session.get_node_or_null("car") as RigidBody3D
	if car:
		print("speed: %.1f m/s" % car.linear_velocity.length())
