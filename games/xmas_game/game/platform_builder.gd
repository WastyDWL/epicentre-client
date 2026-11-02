class_name PlatformBuilder




const COLLISION_DEPTH: = 2.0
const FLOAT_LIMIT: = 2.0
const MODULE: = 2.0

const MODULE_DEPTH: = 1.85


static func build(rect: Rect2, parent: Node3D) -> StaticBody3D:
	var w: = rect.size.x
	var h: = rect.size.y
	var floating: = h <= FLOAT_LIMIT
	var body: = StaticBody3D.new()
	body.position = Vector3(rect.position.x + w * 0.5, rect.position.y - h * 0.5, 0.0)
	body.set_meta("end_x", rect.end.x)

	var shape: = CollisionShape3D.new()
	var box: = BoxShape3D.new()
	box.size = Vector3(w, h, COLLISION_DEPTH)
	shape.shape = box
	body.add_child(shape)

	var top: = h * 0.5
	_tiles(body, "plat_float" if floating else "plat_mid", - w * 0.5, w, top)
	if not floating:
		var end_mesh: = Models.mesh("plat_end")
		var left: = MeshInstance3D.new()
		left.mesh = end_mesh
		left.position = Vector3( - w * 0.5, top, 0.0)
		body.add_child(left)
		var right: = MeshInstance3D.new()
		right.mesh = end_mesh
		right.position = Vector3(w * 0.5, top, 0.0)

		right.rotation.y = PI
		body.add_child(right)
		var cliff_h: = h - MODULE_DEPTH
		if cliff_h > 0.0:
			GameMaterials.mesh(GameMaterials.box(Vector3(w - 0.05, cliff_h, 3.9)), &"ice_dark", body, 
				Vector3(0, top - MODULE_DEPTH - cliff_h * 0.5, 0), false)

	parent.add_child(body)
	return body



static func _tiles(body: Node3D, module: String, x0: float, w: float, top: float) -> void :
	var count: = maxi(1, ceili(w / MODULE - 0.01))
	var mm: = MultiMesh.new()
	mm.transform_format = MultiMesh.TRANSFORM_3D
	mm.mesh = Models.mesh(module)
	mm.instance_count = count
	for i in count:
		var x: = x0 + i * MODULE
		var span: = minf(MODULE, x0 + w - x)
		var basis: = Basis().scaled(Vector3(span / MODULE, 1, 1))
		mm.set_instance_transform(i, Transform3D(basis, Vector3(x, top, 0)))
	var mmi: = MultiMeshInstance3D.new()
	mmi.multimesh = mm
	body.add_child(mmi)
