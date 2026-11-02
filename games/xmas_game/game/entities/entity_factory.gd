class_name EntityFactory



const GIFTS: = ["gift_red", "gift_green", "gift_gold"]


static func spawn(kind: StringName, pos: Vector2, parent: Node3D, player: Node3D) -> RunEntity:
	var e: = RunEntity.new()
	e.kind = kind
	e.player = player
	e.position = Vector3(pos.x, pos.y, 0.0)
	e.set_meta("end_x", pos.x + 2.0)
	e.model = Node3D.new()
	e.add_child(e.model)
	match kind:
		&"gift":
			e.height = 0.6
			Models.place(GIFTS[randi() % GIFTS.size()], e.model, Vector3.ZERO, true, 0.0, 1.1)
			_shape(e, Vector3(0.8, 0.8, 2.0), Vector3.ZERO)
		&"crate":
			e.height = 1.0
			Models.place("crate", e.model, Vector3.ZERO)


			_shape(e, Vector3(1.3, 1.25, 2.0), Vector3(0, 0.55, 0))
			var body: = StaticBody3D.new()
			body.name = "Solid"
			e.add_child(body)
			var cs: = CollisionShape3D.new()
			cs.shape = BoxShape3D.new()
			cs.shape.size = Vector3(1.0, 1.0, 2.0)
			cs.position = Vector3(0, 0.5, 0)
			body.add_child(cs)
		&"snowman":
			e.height = 1.7
			Models.place("snowman", e.model, Vector3.ZERO, true, 0.0, 0.95)
			_shape(e, Vector3(0.9, 1.7, 2.0), Vector3(0, 0.85, 0))
		&"crow":
			e.height = 0.6
			var crow: = Models.scene("crow").instantiate()
			crow.scale = Vector3.ONE * 1.2
			e.model.add_child(crow)
			_shape(e, Vector3(0.9, 0.6, 2.0), Vector3.ZERO)
		&"spikes":
			e.height = 0.7
			Models.place("spikes", e.model, Vector3.ZERO)
			_shape(e, Vector3(1.3, 0.6, 2.0), Vector3(0, 0.3, 0))
	parent.add_child(e)
	return e


static func _shape(e: RunEntity, size: Vector3, center: Vector3) -> void :
	var cs: = CollisionShape3D.new()
	var box: = BoxShape3D.new()
	box.size = size
	cs.shape = box
	cs.position = center
	e.add_child(cs)
