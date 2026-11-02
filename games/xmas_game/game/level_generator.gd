class_name LevelGenerator
extends RefCounted




const START_LENGTH: = 40.0
const GROUND_DEPTH: = 12.0
const FLOAT_THICKNESS: = 0.8
const MIN_Y: = -2.0
const MAX_Y: = 6.0

const FULL_DIFFICULTY_AT: = 1500.0

var end_x: = -10.0
var end_y: = 0.0
var _rng: = RandomNumberGenerator.new()
var _chunks_made: = 0


func _init(seed: int) -> void :
	_rng.seed = seed


func next_chunk(distance: float) -> Dictionary:
	var chunk: = {"platforms": [] as Array[Rect2], "spawns": [] as Array[Dictionary]}
	if _chunks_made == 0:
		_add_platform(chunk, 0.0, START_LENGTH + 10.0, 0.0, GROUND_DEPTH)
		_gift_row(chunk, 12.0, 0.0, 4)
		_gift_row(chunk, 26.0, 0.0, 3)
	else:
		var d: = clampf(distance / FULL_DIFFICULTY_AT, 0.0, 1.0)
		match _pick_pattern(d):
			&"flat":
				_flat(chunk, d)
			&"gap":
				_gap(chunk, d)
			&"steps_up":
				_steps(chunk, d, 1.0)
			&"steps_down":
				_steps(chunk, d, -1.0)
			&"roof":
				_roof(chunk, d)
			&"floating":
				_floating(chunk, d)
			&"spike_field":
				_spike_field(chunk, d)
			&"crow_flock":
				_crow_flock(chunk, d)
			&"crate_stack":
				_crate_stack(chunk, d)
	_chunks_made += 1
	return chunk


func _pick_pattern(d: float) -> StringName:
	var weights: = {
		&"flat": lerpf(3.0, 1.0, d), 
		&"gap": lerpf(2.0, 3.0, d), 
		&"steps_up": 1.5, 
		&"steps_down": 1.5, 
		&"roof": 1.0, 
		&"floating": lerpf(0.5, 2.0, d), 
		&"spike_field": lerpf(0.3, 1.5, d), 
		&"crow_flock": lerpf(0.2, 1.2, d), 
		&"crate_stack": 0.8, 
	}

	if end_y >= MAX_Y - 1.0:
		weights[&"steps_up"] = 0.0
		weights[&"roof"] = 0.0
	if end_y <= MIN_Y + 1.0:
		weights[&"steps_down"] = 0.0
	var total: = 0.0
	for w in weights.values():
		total += w
	var roll: = _rng.randf() * total
	for k in weights:
		roll -= weights[k]
		if roll <= 0.0:
			return k
	return &"flat"



func _gap_width(d: float) -> float:
	var limit: = RunRules.max_gap(RunRules.SPEED_MIN)
	return _rng.randf_range(limit * 0.35, limit * lerpf(0.6, 1.0, d))


func _add_platform(chunk: Dictionary, x: float, width: float, top: float, thickness: float) -> Rect2:
	var rect: = Rect2(x, top, width, thickness)
	chunk.platforms.append(rect)
	end_x = rect.end.x
	end_y = top
	return rect


func _ground(chunk: Dictionary, gap: float, width: float, top: float) -> Rect2:
	return _add_platform(chunk, end_x + gap, width, clampf(top, MIN_Y, MAX_Y), GROUND_DEPTH)


func _flat(chunk: Dictionary, d: float) -> void :
	var p: = _ground(chunk, 0.0, _rng.randf_range(18.0, 28.0), end_y)
	_populate(chunk, p, d, 2)


func _gap(chunk: Dictionary, d: float) -> void :
	var drop: = _rng.randf_range(-1.5, 0.0) if _rng.randf() < 0.4 else 0.0
	var gap_start: = end_x
	var from_y: = end_y
	var p: = _ground(chunk, _gap_width(d), _rng.randf_range(12.0, 22.0), end_y + drop)

	if _rng.randf() < 0.6:
		_gift_arc(chunk, gap_start, p.position.x, maxf(from_y, p.position.y))
	_populate(chunk, p, d, 1)



func _spike_field(chunk: Dictionary, d: float) -> void :
	var count: = _rng.randi_range(2, 3 if d > 0.4 else 2)
	var spacing: = 6.0
	var p: = _ground(chunk, 0.0, 8.0 + count * spacing, end_y)
	for i in count:
		var x: = p.position.x + 6.0 + i * spacing
		chunk.spawns.append({"kind": &"spikes", "pos": Vector2(x, p.position.y)})
		chunk.spawns.append({"kind": &"gift", "pos": Vector2(x, p.position.y + 2.6)})



func _crow_flock(chunk: Dictionary, d: float) -> void :
	var p: = _ground(chunk, 0.0, _rng.randf_range(20.0, 26.0), end_y)
	var count: = _rng.randi_range(2, 3)
	for i in count:
		chunk.spawns.append({"kind": &"crow", "pos": Vector2(p.position.x + 10.0 + i * 5.0, p.position.y + [2.2, 3.4, 2.8][i])})
	_gift_row(chunk, p.position.x + 4.0, p.position.y, 3)



func _crate_stack(chunk: Dictionary, d: float) -> void :
	var p: = _ground(chunk, 0.0, _rng.randf_range(14.0, 20.0), end_y)
	var x: = p.position.x + p.size.x * 0.5
	chunk.spawns.append({"kind": &"crate", "pos": Vector2(x, p.position.y)})
	chunk.spawns.append({"kind": &"crate", "pos": Vector2(x, p.position.y + 1.0)})
	if d > 0.3:
		chunk.spawns.append({"kind": &"crate", "pos": Vector2(x + 1.0, p.position.y)})


func _gift_arc(chunk: Dictionary, from_x: float, to_x: float, top: float) -> void :
	var count: = 4
	for i in count:
		var t: = (i + 0.5) / count
		chunk.spawns.append({"kind": &"gift", "pos": Vector2(lerpf(from_x, to_x, t), top + 1.2 + sin(t * PI) * 1.8)})


func _steps(chunk: Dictionary, d: float, dir: float) -> void :
	for i in _rng.randi_range(2, 3):
		var rise: = _rng.randf_range(0.8, RunRules.max_step_up()) if dir > 0.0 else - _rng.randf_range(0.8, 2.5)
		var p: = _ground(chunk, 0.0, _rng.randf_range(6.0, 10.0), end_y + rise)
		if i == 0:
			_populate(chunk, p, d, 1)


func _roof(chunk: Dictionary, d: float) -> void :
	var rise: = _rng.randf_range(1.5, RunRules.max_step_up())
	var p: = _ground(chunk, 0.0, _rng.randf_range(16.0, 24.0), end_y + rise)
	_populate(chunk, p, d, 3)


func _floating(chunk: Dictionary, d: float) -> void :
	var base: = end_y
	var limit: = RunRules.max_gap(RunRules.SPEED_MIN) * 0.7
	for i in _rng.randi_range(2, 4):
		var top: = clampf(base + _rng.randf_range(-1.0, RunRules.max_step_up() * 0.8), MIN_Y, MAX_Y)
		if top - end_y > RunRules.max_step_up():
			top = end_y + RunRules.max_step_up()
		var p: = _add_platform(chunk, end_x + _rng.randf_range(limit * 0.4, limit), _rng.randf_range(4.0, 7.0), top, FLOAT_THICKNESS)
		_gift_row(chunk, p.position.x + 1.0, top, 2)

	var g: = _ground(chunk, _rng.randf_range(limit * 0.4, limit), _rng.randf_range(10.0, 16.0), end_y - _rng.randf_range(0.0, 1.0))
	_populate(chunk, g, d, 1)



func _populate(chunk: Dictionary, p: Rect2, d: float, slots: int) -> void :
	var usable: = p.size.x - 6.0
	if usable <= 2.0:
		return
	var step: = usable / slots
	for i in slots:
		var x: = p.position.x + 3.0 + step * (i + 0.5)
		var roll: = _rng.randf()
		var enemy_chance: = lerpf(0.25, 0.65, d)
		if roll < enemy_chance * 0.5 and p.size.x >= 10.0:

			chunk.spawns.append({"kind": &"snowman", "pos": Vector2(x, p.position.y), "hop": _rng.randf() < d * 0.6})
		elif roll < enemy_chance * 0.75:
			chunk.spawns.append({"kind": &"spikes", "pos": Vector2(x, p.position.y)})
		elif roll < enemy_chance:
			chunk.spawns.append({"kind": &"crow", "pos": Vector2(x + 6.0, p.position.y + _rng.randf_range(2.0, 3.2))})
		elif roll < enemy_chance + 0.2:
			chunk.spawns.append({"kind": &"crate", "pos": Vector2(x, p.position.y)})
		else:
			_gift_row(chunk, x - 1.0, p.position.y, 3)


func _gift_row(chunk: Dictionary, x: float, top: float, count: int) -> void :
	for i in count:
		chunk.spawns.append({"kind": &"gift", "pos": Vector2(x + i * 1.0, top + 1.1)})
