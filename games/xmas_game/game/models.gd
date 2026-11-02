class_name Models



const DIR: = "res://assets/models/%s.glb"

static var _scenes: = {}
static var _meshes: = {}


static func scene(name: String) -> PackedScene:
	if not _scenes.has(name):
		_scenes[name] = load(DIR % name)
	return _scenes[name]


static func mesh(name: String) -> Mesh:
	if not _meshes.has(name):
		var root: = scene(name).instantiate()
		var found: = root.find_children("*", "MeshInstance3D", true, false)
		_meshes[name] = (found[0] as MeshInstance3D).mesh if not found.is_empty() else null
		root.free()
	return _meshes[name]


static func place(name: String, parent: Node3D, pos: Vector3, shadows: = true, rot_y: = 0.0, s: = 1.0) -> MeshInstance3D:
	var mi: = MeshInstance3D.new()
	mi.mesh = mesh(name)
	mi.position = pos
	mi.rotation.y = rot_y
	mi.scale = Vector3.ONE * s
	if not shadows:
		mi.cast_shadow = GeometryInstance3D.SHADOW_CASTING_SETTING_OFF
	parent.add_child(mi)
	return mi
