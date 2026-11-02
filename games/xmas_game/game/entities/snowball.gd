class_name Snowball
extends Area3D


signal hit(target: RunEntity)

const SPEED: = 22.0
const LIFT: = 0.0
const GRAVITY: = 3.0
const LIFETIME: = 1.2

var velocity: = Vector3(SPEED, LIFT, 0.0)
var _age: = 0.0


static func create(from: Vector3, extra_speed: float) -> Snowball:
	var s: = Snowball.new()
	s.position = from
	s.velocity.x += extra_speed
	s.collision_layer = 0
	s.collision_mask = 1 | RunEntity.ENTITY_LAYER
	var cs: = CollisionShape3D.new()
	var sphere: = SphereShape3D.new()
	sphere.radius = 0.25
	cs.shape = sphere
	s.add_child(cs)
	Models.place("snowball", s, Vector3(0, 0, RunEntity.FRONT_Z))
	return s


func _ready() -> void :
	area_entered.connect(_on_area_entered)
	body_entered.connect(_on_body_entered)


func _physics_process(delta: float) -> void :
	_age += delta
	velocity.y -= GRAVITY * delta
	position += velocity * delta
	if _age > LIFETIME:
		queue_free()


func _on_area_entered(area: Area3D) -> void :
	if area is RunEntity and area.kind in [&"crate", &"snowman", &"crow"] and not area.is_queued_for_deletion():
		hit.emit(area)
		queue_free()


func _on_body_entered(body: Node3D) -> void :
	if body.get_parent() is RunEntity:
		_on_area_entered(body.get_parent())
	elif body is StaticBody3D:
		queue_free()
