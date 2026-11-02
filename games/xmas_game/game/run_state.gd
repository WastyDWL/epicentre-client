class_name RunState
extends RefCounted


signal changed()
signal damaged(lives_left: int)
signal game_over()

const ENEMY_KINDS: = [&"snowman", &"snowman_stomp", &"crow"]

var score: = 0
var distance: = 0.0
var gifts: = 0
var crates: = 0
var enemies: = 0
var lives: = RunRules.LIVES
var multiplier: = 1
var duration: = 0.0
var is_over: = false

var _action_points: = 0
var _actions_since_hit: = 0
var _last_hit: = - INF


func tick(delta: float) -> void :
	if not is_over:
		duration += delta


func add_distance(metres: float) -> void :
	if is_over:
		return
	var before: = int(distance)
	distance += metres
	if int(distance) != before:
		_recount()


func award(kind: StringName) -> void :
	if is_over:
		return
	match kind:
		&"gift":
			gifts += 1
		&"crate":
			crates += 1
		_:
			if kind in ENEMY_KINDS:
				enemies += 1
	_action_points += RunRules.points(kind) * multiplier
	_actions_since_hit += 1
	multiplier = RunRules.multiplier(_actions_since_hit)
	_recount()



func take_hit(now: float) -> bool:
	if is_over or now - _last_hit < RunRules.INVULNERABLE:
		return false
	_last_hit = now
	lives -= 1
	_actions_since_hit = 0
	multiplier = 1
	damaged.emit(lives)
	if lives <= 0:
		is_over = true
		game_over.emit()
	changed.emit()
	return true


func to_payload() -> Dictionary:
	return {
		"score": score, 
		"distance": int(distance), 
		"gifts": gifts, 
		"crates": crates, 
		"enemies": enemies, 
		"duration": int(duration), 
	}


func _recount() -> void :
	score = int(distance) + _action_points
	changed.emit()
