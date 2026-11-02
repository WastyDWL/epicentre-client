extends Node3D






const EASE: = 16.0
const AIR_BLEND: = 10.0
const HIPS_REST_Y: = 0.42
const RUN_FREQ: = 1.55
const THROW_TIME: = 0.32
const HIT_TIME: = 0.45

const GESTURES: = {&"drink": 1.9, &"hic": 0.5, &"shrug": 1.4, &"point": 1.6, &"cheer": 2.0}

var _pivots: = {}
var _meshes: Array[MeshInstance3D] = []
var _phase: = 0.0
var _time: = 0.0
var _air: = 0.0
var _throw_t: = -1.0
var _hit_t: = -1.0

var drunk: = false

var talking: = false
var _gesture: = &""
var _gesture_t: = -1.0
var _overlay: = StandardMaterial3D.new()


func _ready() -> void :
	for n in ["Hips", "Head", "ArmL", "ForearmL", "ArmR", "ForearmR", "LegL", "ShinL", "LegR", "ShinR"]:
		_pivots[n] = find_child(n, true, false)
	for mi in find_children("*", "MeshInstance3D", true, false):
		if mi.name.ends_with("Outline"):

			mi.cast_shadow = GeometryInstance3D.SHADOW_CASTING_SETTING_OFF
		else:
			_meshes.append(mi)
	_overlay.shading_mode = BaseMaterial3D.SHADING_MODE_UNSHADED
	_overlay.transparency = BaseMaterial3D.TRANSPARENCY_ALPHA
	_overlay.albedo_color = Color(1.0, 0.2, 0.2, 0.0)


func has_all_pivots() -> bool:
	for p in _pivots.values():
		if p == null:
			return false
	return true


func throw() -> void :
	_throw_t = 0.0


func gesture(name: StringName) -> void :
	if GESTURES.has(name):
		_gesture = name
		_gesture_t = 0.0


func is_gesturing() -> bool:
	return _gesture_t >= 0.0


func hit() -> void :
	_hit_t = 0.0
	for mi in _meshes:
		mi.material_overlay = _overlay
	_overlay.albedo_color.a = 0.55
	var t: = create_tween()
	t.tween_property(_overlay, "albedo_color:a", 0.0, 0.35)
	t.tween_callback( func() -> void :
		for mi in _meshes:
			mi.material_overlay = null)



func animate(delta: float, on_floor: bool, vel_y: float, speed: float, running: bool) -> void :
	_time += delta
	_air = move_toward(_air, 0.0 if on_floor else 1.0, AIR_BLEND * delta)
	var pose: = {}
	if running:
		_phase += delta * TAU * RUN_FREQ * clampf(speed / RunRules.SPEED_MIN, 0.8, 1.9)
		pose = _run_pose()
	elif drunk:
		pose = _drunk_pose()
	else:
		pose = _idle_pose()
	if _air > 0.0:
		var rise: = clampf(inverse_lerp( - RunRules.JUMP_VELOCITY, RunRules.JUMP_VELOCITY, vel_y), 0.0, 1.0)
		pose = _mix(pose, _mix(_fall_pose(), _rise_pose(), rise), smoothstep(0.0, 1.0, _air))
	_apply_gesture(pose, delta)
	_apply_throw(pose, delta)
	_apply_hit(pose, delta)
	var k: = 1.0 - exp( - EASE * delta)
	for n in pose:
		if n == "HipsY":
			_pivots["Hips"].position.y = lerpf(_pivots["Hips"].position.y, pose[n], k)
		else:
			var p: Node3D = _pivots[n]
			p.rotation.z = lerp_angle(p.rotation.z, pose[n], k)


func _run_pose() -> Dictionary:
	var s: = sin(_phase)
	var c: = cos(_phase)
	return {
		"LegL": 0.75 * s, "LegR": -0.75 * s, 

		"ShinL": -0.15 - 1.0 * maxf(0.0, sin(_phase + 1.3)), 
		"ShinR": -0.15 - 1.0 * maxf(0.0, sin(_phase + 1.3 + PI)), 
		"ArmL": -0.65 * s, "ArmR": 0.65 * s, 
		"ForearmL": 0.55 + 0.25 * maxf(0.0, - s), "ForearmR": 0.55 + 0.25 * maxf(0.0, s), 
		"Hips": -0.12 + 0.03 * sin(_phase * 2.0), 
		"HipsY": HIPS_REST_Y + 0.045 * absf(c), 
		"Head": 0.08 + 0.03 * sin(_phase * 2.0 + 0.6), 
	}


func _idle_pose() -> Dictionary:
	var b: = sin(_time * 2.2)
	return {
		"LegL": 0.0, "LegR": 0.0, "ShinL": 0.0, "ShinR": 0.0, 
		"ArmL": 0.06 * b, "ArmR": -0.06 * b, "ForearmL": 0.15, "ForearmR": 0.15, 
		"Hips": 0.0, "HipsY": HIPS_REST_Y + 0.012 * b, "Head": 0.04 * sin(_time * 1.1), 
	}



func _drunk_pose() -> Dictionary:
	var sway: = sin(_time * 1.3)
	var nod: = sin(_time * 9.0) * 0.07 if talking else 0.0
	return {
		"LegL": 0.05 * sway, "LegR": -0.05 * sway, "ShinL": -0.08, "ShinR": -0.08, 
		"ArmL": 0.25 + 0.08 * sway, "ArmR": -0.05 - 0.06 * sway, "ForearmL": 0.55, "ForearmR": 0.25, 
		"Hips": 0.09 * sway, "HipsY": HIPS_REST_Y - 0.01 + 0.012 * sin(_time * 2.6), 
		"Head": -0.06 * sway + 0.05 * sin(_time * 0.7) + nod, 
	}



func _gesture_pose(name: StringName, t: float) -> Dictionary:
	match name:
		&"drink":

			return {"ArmL": 2.35, "ForearmL": 2.05, "Head": -0.35 + 0.05 * sin(t * 30.0), "Hips": -0.08}
		&"hic":
			var jolt: = sin(clampf(t * 2.0, 0.0, 1.0) * PI)
			return {"Hips": 0.18 * jolt, "Head": 0.3 * jolt, "HipsY": HIPS_REST_Y + 0.06 * jolt}
		&"shrug":
			return {"ArmL": 0.7, "ArmR": 0.7, "ForearmL": 1.3, "ForearmR": 1.3, "Head": -0.15}
		&"point":
			return {"ArmR": 1.55, "ForearmR": 0.1, "Head": 0.1, "Hips": 0.06}
		&"cheer":

			return {"ArmL": 2.9, "ForearmL": 0.3, "ArmR": 2.4 + 0.3 * sin(t * 16.0), "ForearmR": 0.6, "Head": -0.12}
	return {}


func _apply_gesture(pose: Dictionary, delta: float) -> void :
	if _gesture_t < 0.0:
		return
	_gesture_t += delta
	var length: float = GESTURES[_gesture]
	var t: = _gesture_t / length
	if t >= 1.0:
		_gesture_t = -1.0
		return

	var w: = smoothstep(0.0, 0.25, t) * (1.0 - smoothstep(0.75, 1.0, t))
	var target: = _gesture_pose(_gesture, t)
	for n in target:
		pose[n] = lerpf(pose.get(n, 0.0), target[n], w)


func _rise_pose() -> Dictionary:
	return {
		"LegL": 1.0, "ShinL": -1.35, "LegR": -0.3, "ShinR": -0.55, 
		"ArmL": 2.3, "ArmR": 1.7, "ForearmL": 0.35, "ForearmR": 0.5, 
		"Hips": -0.05, "HipsY": HIPS_REST_Y + 0.03, "Head": 0.12, 
	}


func _fall_pose() -> Dictionary:
	var w: = sin(_time * 14.0) * 0.12
	return {
		"LegL": 0.45, "ShinL": -0.45, "LegR": -0.1, "ShinR": -0.7, 
		"ArmL": 2.7 + w, "ArmR": 2.5 - w, "ForearmL": 0.2, "ForearmR": 0.25, 
		"Hips": 0.06, "HipsY": HIPS_REST_Y, "Head": -0.05, 
	}


func _mix(a: Dictionary, b: Dictionary, t: float) -> Dictionary:
	var out: = {}
	for n in a:
		out[n] = lerpf(a[n], b.get(n, a[n]), t)
	return out



func _apply_throw(pose: Dictionary, delta: float) -> void :
	if _throw_t < 0.0:
		return
	_throw_t += delta
	var t: = _throw_t / THROW_TIME
	if t >= 1.0:
		_throw_t = -1.0
		return
	var swing: = -1.2 + 3.4 * smoothstep(0.15, 0.55, t)
	var w: = sin(t * PI)
	pose["ArmR"] = lerpf(pose["ArmR"], swing, w)
	pose["ForearmR"] = lerpf(pose["ForearmR"], 0.2, w)
	pose["Hips"] = pose["Hips"] - 0.12 * w



func _apply_hit(pose: Dictionary, delta: float) -> void :
	if _hit_t < 0.0:
		return
	_hit_t += delta
	var t: = _hit_t / HIT_TIME
	if t >= 1.0:
		_hit_t = -1.0
		return
	var w: = sin(t * PI)
	pose["Hips"] = pose["Hips"] + 0.45 * w
	pose["Head"] = pose["Head"] + 0.3 * w
	pose["ArmL"] = lerpf(pose["ArmL"], 1.4, w)
	pose["ArmR"] = lerpf(pose["ArmR"], 1.2, w)
