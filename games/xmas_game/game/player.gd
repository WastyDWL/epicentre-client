extends CharacterBody3D




signal fell_into_pit()
signal jumped()
signal throw_requested(from: Vector3)

const PIT_Y: = -7.0
const MAX_STEP: = 1.0 / 20.0
const COYOTE: = 0.1
const JUMP_BUFFER: = 0.12
const JUMP_CUT: = 0.5

const TURN_TO_CAMERA: = 0.0

@onready var visual: Node3D = $Visual
@onready var rig: Node3D = $Visual / Santa
@onready var puff: CPUParticles3D = $Puff

var speed: = RunRules.SPEED_MIN
var running: = true
var is_invulnerable: = false
var _invulnerable_left: = 0.0
var _air_jumps: = 1
var _since_floor: = 0.0
var _jump_buffer: = 0.0
var _throw_cooldown: = 0.0

var _throw_queued: = false
var _was_on_floor: = true

var fall_speed: = 0.0
var _flipping: = false
var _squash_tween: Tween


func _ready() -> void :
	rig.rotation.y = TURN_TO_CAMERA
	puff.mesh = GameMaterials.sphere(1.0, 6)
	puff.material_override = GameMaterials.get_mat(&"snow")


func _unhandled_input(event: InputEvent) -> void :
	if event is InputEventMouseButton and event.pressed and event.button_index == MOUSE_BUTTON_LEFT:
		_throw_queued = true


func _physics_process(delta: float) -> void :
	step(delta)


func step(delta: float) -> void :
	delta = minf(delta, MAX_STEP)

	if is_on_floor():
		_since_floor = 0.0
		_air_jumps = 1
	else:
		_since_floor += delta
		velocity.y -= RunRules.GRAVITY * delta

	_jump_buffer = maxf(_jump_buffer - delta, 0.0)
	if Input.is_action_just_pressed("jump"):
		_jump_buffer = JUMP_BUFFER
	if _jump_buffer > 0.0:
		if _since_floor <= COYOTE:
			_jump(RunRules.JUMP_VELOCITY, false)
		elif _air_jumps > 0:
			_air_jumps -= 1
			_jump(RunRules.DOUBLE_JUMP_VELOCITY, true)

	if Input.is_action_just_released("jump") and velocity.y > 0.0:
		velocity.y *= JUMP_CUT

	_throw_cooldown = maxf(_throw_cooldown - delta, 0.0)
	var wants_throw: = Input.is_action_just_pressed("throw") or _throw_queued
	_throw_queued = false
	if wants_throw and _throw_cooldown <= 0.0:
		_throw_cooldown = RunRules.SNOWBALL_COOLDOWN
		throw_requested.emit(global_position + Vector3(0.6, 0.75, 0.0))
		rig.throw()

	velocity.x = speed if running else 0.0
	velocity.z = 0.0
	fall_speed = - velocity.y

	var scale_to_tick: = delta * Engine.physics_ticks_per_second
	velocity *= scale_to_tick
	move_and_slide()
	velocity /= scale_to_tick
	global_position.z = 0.0

	var on_floor: = is_on_floor()
	if on_floor and not _was_on_floor:
		_land()
	_was_on_floor = on_floor

	_update_invulnerability(delta)
	rig.animate(delta, on_floor, velocity.y, speed, running)

	if global_position.y < PIT_Y:
		fell_into_pit.emit()


func _jump(strength: float, double: bool) -> void :
	velocity.y = strength
	_jump_buffer = 0.0
	_since_floor = COYOTE + 1.0
	_squash(Vector3(0.86, 1.16, 1.0), 0.18)
	if double:
		_flip()
	jumped.emit()



func bounce() -> void :
	velocity.y = RunRules.JUMP_VELOCITY * 0.8
	_air_jumps = 1
	_squash(Vector3(0.88, 1.14, 1.0), 0.18)


func respawn(at: Vector3) -> void :
	global_position = at
	velocity = Vector3.ZERO
	_was_on_floor = true
	reset_physics_interpolation()
	set_invulnerable(RunRules.INVULNERABLE)


func set_invulnerable(seconds: float) -> void :
	is_invulnerable = true
	_invulnerable_left = seconds

	rig.hit()


func _land() -> void :
	var hard: = clampf(fall_speed / 18.0, 0.25, 1.0)
	_squash(Vector3(1.0 + 0.2 * hard, 1.0 - 0.22 * hard, 1.0), 0.2)
	puff.amount = int(6 + 8 * hard)
	puff.restart()


func _squash(to: Vector3, seconds: float) -> void :
	if _squash_tween:
		_squash_tween.kill()
	visual.scale = to
	_squash_tween = create_tween().set_trans(Tween.TRANS_BACK).set_ease(Tween.EASE_OUT)
	_squash_tween.tween_property(visual, "scale", Vector3.ONE, seconds)


func _flip() -> void :
	_flipping = true
	visual.rotation.z = 0.0
	var t: = create_tween().set_trans(Tween.TRANS_SINE).set_ease(Tween.EASE_OUT)
	t.tween_property(visual, "rotation:z", - TAU, 0.42)
	t.tween_callback( func() -> void :
		visual.rotation.z = 0.0
		_flipping = false)


func _update_invulnerability(delta: float) -> void :
	if not is_invulnerable:
		return
	_invulnerable_left -= delta
	rig.visible = int(_invulnerable_left * 12.0) % 2 == 0
	if _invulnerable_left <= 0.0:
		is_invulnerable = false
		rig.visible = true
