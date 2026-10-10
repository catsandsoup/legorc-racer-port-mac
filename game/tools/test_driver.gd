# Automated smoke test / screenshot driver. Inactive unless SSRC_TEST is set.
#   SSRC_TEST=1                       enable
#   SSRC_TEST_FLOW=title,select,help,mode:0   screens to click through (or mode:N to jump)
#   SSRC_TEST_INPUT=accelerate:1-6,steer_left:3-4   action windows (seconds after driving starts)
#   SSRC_TEST_SHOTS=0.5,2,5           screenshot times (seconds after driving starts; t<0 = menus)
#   SSRC_TEST_OUT=/path/prefix        screenshot path prefix
#   SSRC_TEST_QUIT=8                  quit after N seconds of driving
#   SSRC_TEST_TELEMETRY=/path.csv     write the car state every frame (Director units)
extends Node

var enabled := false
var t_drive := -1.0
var sim_start := -1.0
var t_total := 0.0
var shots: Array = []
var inputs: Array = []
var quit_after := 10.0
var out := "/tmp/ssrc"
var game: Node
var flow: Array = []
var flow_t := 0.0
var telemetry: FileAccess

func _ready() -> void:
	enabled = OS.get_environment("SSRC_TEST") != ""
	if not enabled:
		set_process(false)
		return
	process_mode = Node.PROCESS_MODE_ALWAYS
	out = OS.get_environment("SSRC_TEST_OUT") if OS.get_environment("SSRC_TEST_OUT") != "" else out
	quit_after = float(OS.get_environment("SSRC_TEST_QUIT")) if OS.get_environment("SSRC_TEST_QUIT") != "" else 10.0
	for s in OS.get_environment("SSRC_TEST_SHOTS").split(",", false):
		shots.append(float(s))
	for w in OS.get_environment("SSRC_TEST_INPUT").split(",", false):
		var p = w.split(":")
		var r = p[1].split("-")
		inputs.append([p[0], float(r[0]), float(r[1]), false])
	flow = Array(OS.get_environment("SSRC_TEST_FLOW").split(",", false))
	if OS.get_environment("SSRC_TEST_TELEMETRY") != "":
		telemetry = FileAccess.open(OS.get_environment("SSRC_TEST_TELEMETRY"), FileAccess.WRITE)
		telemetry.store_line("t,sim,px,py,pz,cx,cy,cz,vx,vy,vz,wx,wy,wz,xx,xy,xz,yx,yy,yz,zx,zy,zz")

func log_state() -> void:
	var st: Dictionary = game.get_session().get_car_state()
	if st.is_empty():
		return
	var row := [t_drive, st.sim_time]
	for k in ["pos", "com", "vel", "angvel", "x_axis", "y_axis", "z_axis"]:
		var v: Vector3 = st[k]
		row.append_array([v.x, v.y, v.z])
	telemetry.store_line(",".join(row.map(func(x): return "%.5f" % x)))

func shot(tag: String) -> void:
	var img := get_viewport().get_texture().get_image()
	img.save_png("%s_%s.png" % [out, tag])
	print("[test] shot ", tag, " ", img.get_size(), " fps=", Engine.get_frames_per_second())

func _process(delta: float) -> void:
	if game == null:
		game = get_tree().root.get_node_or_null("SupersonicGame")
		return
	var real_dt: float = delta / max(Engine.time_scale, 0.0001)
	t_total += real_dt
	if t_drive < 0:
		flow_t += real_dt
		if game.get_session() != null and game.is_driving():
			t_drive = 0.0
			print("[test] driving started at ", t_total)
		elif flow.size() > 0 and flow_t > 2.5:
			var step: String = flow.pop_front()
			flow_t = 0.0
			print("[test] flow step ", step, " at ", t_total)
			if step.begins_with("mode:"):
				game.start_mode(int(step.substr(5)))
			elif step.begins_with("shot:"):
				shot(step.substr(5))
			else:
				var ev := InputEventMouseButton.new()
				ev.button_index = MOUSE_BUTTON_LEFT
				# Stage (640x480) -> window pixels through the stretch transform.
				var vs := get_viewport().get_visible_rect().size
				var stage := Vector2(320, 240)
				if step.begins_with("click:"):
					var xy = step.substr(6).split("x")
					stage = Vector2(float(xy[0]), float(xy[1]))
				ev.position = get_viewport().get_final_transform() * ((vs - Vector2(640, 480)) / 2 + stage)
				Input.warp_mouse(ev.position)
				var mm := InputEventMouseMotion.new()
				mm.position = ev.position
				Input.parse_input_event(mm)
				ev.pressed = true
				Input.parse_input_event(ev)
				var up := ev.duplicate()
				up.pressed = false
				Input.parse_input_event(up)
		if t_total > 240:
			print("[test] timeout before driving")
			get_tree().quit(2)
		return
	while shots.size() > 0 and t_drive >= shots[0]:
		shot("drive_%05.2f" % shots.pop_front())

# Inputs, telemetry and the test clock run on the game's own logic ticks
# (game time: simulated time over the original's 2.0x), so port runs repeat exactly whatever
# the frame rate.
func _physics_process(_delta: float) -> void:
	if t_drive < 0 or game == null or game.get_session() == null:
		return
	var st: Dictionary = game.get_session().get_car_state()
	if st.is_empty():
		return
	if sim_start < 0:
		# Spawn on a movie-frame boundary so every run steps identically.
		if st.frame_tick != 0:
			return
		sim_start = st.game_time
		# SSRC_TEST_SPAWN: place the car again on this tick, so time 0 is
		# the moment of spawning (as when teleporting the original's car).
		var sp = OS.get_environment("SSRC_TEST_SPAWN").split_floats(",")
		if sp.size() >= 3:
			game.get_session().test_spawn(Vector3(sp[0], sp[1], sp[2]), sp[3] if sp.size() > 3 else 0.0)
	t_drive = st.game_time - sim_start
	if telemetry != null:
		log_state()
	for i in inputs:
		var on: bool = t_drive >= i[1] and t_drive < i[2]
		if on and not i[3]:
			Input.action_press(i[0])
		elif not on and i[3]:
			Input.action_release(i[0])
		i[3] = on
	if t_drive > quit_after:
		if telemetry != null:
			telemetry.close()
		print("[test] done")
		get_tree().quit(0)
