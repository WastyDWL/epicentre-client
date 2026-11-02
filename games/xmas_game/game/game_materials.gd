class_name GameMaterials


static var _cache: = {}

const COLORS: = {
	&"ice": Color(0.6, 0.8, 0.97), 
	&"ice_dark": Color(0.3, 0.47, 0.72), 
	&"ice_float": Color(0.55, 0.8, 0.95), 
	&"snow": Color(0.93, 0.96, 1.0), 
	&"icicle": Color(0.75, 0.9, 1.0), 
	&"trunk": Color(0.33, 0.22, 0.16), 
	&"pine": Color(0.12, 0.33, 0.3), 
	&"pine_far": Color(0.16, 0.26, 0.38), 
	&"house": Color(0.55, 0.32, 0.26), 
	&"roof": Color(0.88, 0.92, 1.0), 
	&"window": Color(1.0, 0.78, 0.4), 
	&"mountain": Color(0.24, 0.3, 0.48), 
	&"field": Color(0.62, 0.7, 0.86), 
	&"carrot": Color(1.0, 0.5, 0.12), 
	&"coal": Color(0.08, 0.08, 0.1), 
	&"crow": Color(0.12, 0.12, 0.16), 
	&"beak": Color(1.0, 0.75, 0.2), 
	&"crate": Color(0.6, 0.85, 1.0), 
	&"gift_red": Color(0.98, 0.28, 0.29), 
	&"gift_green": Color(0.2, 0.72, 0.45), 
	&"gift_gold": Color(1.0, 0.76, 0.25), 
	&"ribbon": Color(1.0, 0.95, 0.85), 
	&"spike": Color(0.7, 0.88, 1.0), 
	&"snowball": Color(1.0, 1.0, 1.0), 
}

const EMISSIVE: = [&"window", &"gift_gold"]
const TRANSLUCENT: = [&"crate", &"spike", &"ice_float"]


static func get_mat(name: StringName) -> StandardMaterial3D:
	if _cache.has(name):
		return _cache[name]
	var m: = StandardMaterial3D.new()
	m.albedo_color = COLORS[name]
	m.roughness = 0.85
	if name in EMISSIVE:
		m.emission_enabled = true
		m.emission = COLORS[name]
		m.emission_energy_multiplier = 1.2 if name == &"window" else 0.25
	if name in TRANSLUCENT:
		m.roughness = 0.2
		m.metallic_specular = 0.8
	_cache[name] = m
	return m


static func mesh(m: Mesh, mat: StringName, parent: Node3D, pos: = Vector3.ZERO, shadows: = true) -> MeshInstance3D:
	var mi: = MeshInstance3D.new()
	mi.mesh = m
	mi.material_override = get_mat(mat)
	mi.position = pos
	if not shadows:
		mi.cast_shadow = GeometryInstance3D.SHADOW_CASTING_SETTING_OFF
	parent.add_child(mi)
	return mi


static func box(size: Vector3) -> BoxMesh:
	var b: = BoxMesh.new()
	b.size = size
	return b


static func cone(radius: float, height: float, segments: = 7) -> CylinderMesh:
	var c: = CylinderMesh.new()
	c.top_radius = 0.0
	c.bottom_radius = radius
	c.height = height
	c.radial_segments = segments
	c.rings = 1
	return c


static func sphere(radius: float, segments: = 10) -> SphereMesh:
	var s: = SphereMesh.new()
	s.radius = radius
	s.height = radius * 2.0
	s.radial_segments = segments
	s.rings = segments / 2
	return s
