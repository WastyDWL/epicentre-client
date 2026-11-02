class_name Decor




const FIELD_Y: = -2.5
const PINES: = ["pine_a", "pine_b", "pine_a", "pine_b", "pine_lights"]


static func populate(rng: RandomNumberGenerator, from_x: float, to_x: float, parent: Node3D) -> void :
	var x: = from_x
	while x < to_x:
		var z: = rng.randf_range(-7.0, -24.0)
		var roll: = rng.randf()
		if roll < 0.16:
			_house(Vector3(x, FIELD_Y, z - 5.0), rng, parent)
			x += rng.randf_range(9.0, 13.0)
		elif roll < 0.24:
			_prop("rock", Vector3(x, FIELD_Y, z), rng.randf_range(0.8, 1.4), rng, parent)
			x += rng.randf_range(2.0, 4.0)
		else:
			var name: String = PINES[rng.randi() % PINES.size()]
			_prop(name, Vector3(x, FIELD_Y, z), rng.randf_range(0.85, 1.35), rng, parent, z > -12.0)
			x += rng.randf_range(2.5, 5.5)

	if rng.randf() < (to_x - from_x) / 80.0:
		var m: = _root(Vector3(rng.randf_range(from_x, to_x) + 40.0, FIELD_Y - 2.0, rng.randf_range(-115.0, -135.0)), 190.0, parent)
		Models.place("mountains", m, Vector3.ZERO, false, 0.0, rng.randf_range(0.8, 1.1))


static func _root(pos: Vector3, reach: float, parent: Node3D) -> Node3D:
	var n: = Node3D.new()
	n.position = pos
	n.set_meta("end_x", pos.x + reach)
	parent.add_child(n)
	return n


static func _prop(name: String, pos: Vector3, s: float, rng: RandomNumberGenerator, parent: Node3D, shadows: = false) -> void :
	var n: = _root(pos, 4.0, parent)
	Models.place(name, n, Vector3.ZERO, shadows, rng.randf_range(-0.4, 0.4), s)


static func _house(pos: Vector3, rng: RandomNumberGenerator, parent: Node3D) -> void :
	var n: = _root(pos, 8.0, parent)
	Models.place("house_a" if rng.randf() < 0.5 else "house_b", n, Vector3.ZERO, false, rng.randf_range(-0.15, 0.15))
	Models.place("fence", n, Vector3(3.6, 0, 1.2), false)
	Models.place("lamp", n, Vector3(-3.2, 0, 1.6), false)
