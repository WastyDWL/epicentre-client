class_name RunEntity
extends Area3D





signal touched_player(entity: RunEntity, player: CharacterBody3D)

const ENTITY_LAYER: = 2
const FRONT_Z: = 1.2
const PATROL_RANGE: = 2.0
const PATROL_SPEED: = 1.5
const HOP_HEIGHT: = 1.3
const HOP_PERIOD: = 1.1
const CROW_SPEED: = 3.0

const SWOOP_RANGE: = 9.0
const SWOOP_SPEED: = 2.6
const WAKE_DISTANCE: = 28.0

var kind: StringName

var hops: = false

var height: = 1.0
var base: = Vector3.ZERO
var model: Node3D
var player: Node3D
var _time: = 0.0
var _awake: = false
var _dir: = -1.0
var _swoop_y: = 0.0
var _wings: Array[Node] = []


func _ready() -> void :
	collision_layer = ENTITY_LAYER
	collision_mask = 1
	monitorable = true
	base = position
	_swoop_y = base.y
	_time = randf() * TAU
	model.position.z = FRONT_Z
	_wings = model.find_children("Wing?", "", true, false)
	body_entered.connect(_on_body_entered)


func _physics_process(delta: float) -> void :
	_time += delta
	if not _awake:
		_awake = player != null and player.global_position.x > position.x - WAKE_DISTANCE
		if not _awake:
			return
	match kind:
		&"gift":
			model.rotation.y += delta * 2.0
			model.position.y = sin(_time * 3.0) * 0.08
		&"snowman":
			if hops:
				position.y = base.y + absf(sin(_time * PI / HOP_PERIOD)) * HOP_HEIGHT
			else:
				position.x += _dir * PATROL_SPEED * delta
				if absf(position.x - base.x) > PATROL_RANGE:
					_dir = - _dir
					position.x = base.x + signf(position.x - base.x) * PATROL_RANGE
			model.rotation.z = sin(_time * 6.0) * 0.06
		&"crow":
			position.x -= CROW_SPEED * delta
			var ahead: = position.x - player.global_position.x
			if ahead > 0.0 and ahead < SWOOP_RANGE:
				_swoop_y = move_toward(_swoop_y, player.global_position.y + 0.9, SWOOP_SPEED * delta)
			position.y = _swoop_y + sin(_time * 2.5) * 0.5
			for wing in _wings:
				wing.rotation.x = sin(_time * 14.0) * 0.7 * (1.0 if wing.name == "WingL" else -1.0)
	set_meta("end_x", position.x + 2.0)


func is_stomp_from(body: CharacterBody3D) -> bool:

	var falling: bool = body.velocity.y < 0.0 or body.get("fall_speed") > 0.0
	return falling and body.global_position.y > global_position.y + height * 0.5


func _on_body_entered(body: Node3D) -> void :
	if body.is_in_group("player"):
		touched_player.emit(self, body)



func smash() -> void :
	if is_queued_for_deletion():
		return
	var burst: = CPUParticles3D.new()
	burst.one_shot = true
	burst.emitting = true
	burst.amount = 14
	burst.lifetime = 0.6
	burst.explosiveness = 1.0
	burst.mesh = GameMaterials.box(Vector3.ONE * 0.12)
	burst.material_override = GameMaterials.get_mat(&"snow" if kind != &"crate" else &"crate")
	burst.direction = Vector3.UP
	burst.spread = 70.0
	burst.initial_velocity_min = 3.0
	burst.initial_velocity_max = 6.0
	burst.gravity = Vector3(0, -14, 0)
	burst.position = global_position + Vector3(0, height * 0.5, FRONT_Z)
	get_parent().add_child(burst)
	get_tree().create_timer(1.0, false).timeout.connect(burst.queue_free)
	queue_free()
