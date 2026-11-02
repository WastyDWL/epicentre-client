extends Node3D


signal player_fell()

const AHEAD: = 70.0
const BEHIND: = 25.0
const CAMERA_OFFSET: = Vector3(6.0, 2.4, 11.0)
const CAMERA_Y_SMOOTH: = 4.0

const STALL_TIME: = 1.2
const COUNTDOWN: = 3.0

const BASE_PITCH_DEG: = -6.0
const REVEAL_OFFSET: = Vector3(-4.5, 4.2, 7.5)
const REVEAL_IN: = 0.45
const REVEAL_OUT: = 0.6

const WARN_RANGE: = 24.0


const INTRO_CAMERA: = Vector3(1.0, 0.15, 4.8)
const INTRO_TURN_DEG: = -70.0
const INTRO_EASE: = 0.9


@export var level_seed: = 0

@export var spawn_entities: = true

@export var wait_for_start: = true

@export var countdown_without_card: = false

@export var play_intro: = true

var generator_override: LevelGenerator

var bridge: Node = SiteBridge

const HUD_SCENE: = preload("res://game/Hud.tscn")

@onready var player: CharacterBody3D = $Player
@onready var level: Node3D = $Level
@onready var entities: Node3D = $Entities
@onready var decor: Node3D = $Decor
@onready var camera: Camera3D = $Camera3D
@onready var sun: DirectionalLight3D = $Sun
@onready var field: MeshInstance3D = $Field
@onready var music: AudioStreamPlayer = $Music
@onready var sfx_gift: AudioStreamPlayer = $SfxGift
@onready var sfx_kill: AudioStreamPlayer = $SfxKill
@onready var sfx_hit: AudioStreamPlayer = $SfxHit

var hud: CanvasLayer
var started: = false

var counting_down: = false
var generator: LevelGenerator
var state: = RunState.new()
var time: = 0.0
var platforms: Array[Rect2] = []


var _best_x: = 0.0
var _countdown_left: = 0.0
var _stall_time: = 0.0
var _stall_x: = 0.0
var _shake: = 0.0

var _ground_y: = 0.0

var reveal: = 0.0

var intro: Intro

var intro_blend: = 0.0
var _music_db: = 0.0

var _asking: = false
var _ask_countdown: = 0.0
var _gate_mode: = ""
var _decor_rng: = RandomNumberGenerator.new()
var _fps_low: = 0
var _fps_frames: = 0
var _fps_time: = 0.0


func _ready() -> void :
	var seed: = level_seed if level_seed != 0 else randi()
	generator = generator_override if generator_override else LevelGenerator.new(seed)
	_decor_rng.seed = seed + 1
	player.global_position = Vector3(2.0, 0.05, 0.0)
	_best_x = player.global_position.x
	player.fell_into_pit.connect(_on_fell_into_pit)
	player.throw_requested.connect(_on_throw_requested)
	state.damaged.connect(_on_damaged)
	state.game_over.connect(_on_game_over)
	hud = HUD_SCENE.instantiate()
	hud.bridge = bridge
	add_child(hud)
	hud.gate_choice.connect(_on_gate_choice)
	bridge.round_started.connect(_on_round_answer)
	bridge.purchase_done.connect(_on_purchase_done)
	hud.bind(state)
	hud.start_requested.connect(start.bind(COUNTDOWN))
	hud.retry_requested.connect(restart)
	hud.intro_requested.connect(play_cutscene)
	_fill()
	camera.global_position = player.global_position + CAMERA_OFFSET
	camera.reset_physics_interpolation()
	var mp3: = music.stream as AudioStreamMP3
	if mp3:
		mp3.loop = true
	music.play()
	_music_db = music.volume_db
	player.running = false
	if wait_for_start and play_intro and not Intro.was_seen():
		play_cutscene()
		intro_blend = 1.0
	elif wait_for_start:
		hud.show_start(true)
	else:
		start(COUNTDOWN if countdown_without_card else 0.0)




func start(countdown: = 0.0) -> void :
	if started or _asking:
		return
	hud.show_start(false)
	if bridge.is_available():
		_ask_countdown = countdown
		_ask(false)
	else:
		_launch(countdown)


func _ask(confirm_no_xp: bool) -> void :
	_asking = true
	hud.show_gate_wait()
	bridge.start_round(confirm_no_xp)


func _on_round_answer(result: Dictionary) -> void :
	if not _asking:
		return
	_asking = false
	if result.get("ok", false):
		hud.hide_gate()
		_launch(_ask_countdown)
	else:
		_gate_mode = str(result.get("mode", ""))
		hud.show_gate(_gate_mode, result.get("state", {}), str(result.get("message", "")))



func _on_gate_choice(choice: String, count: int) -> void :
	match choice:
		"tickets", "exchange", "extend":
			hud.show_gate_wait()
			bridge.buy(choice, count)
		"no_xp":
			_ask(true)
		_:
			hud.hide_gate()
			hud.show_start(true)



func _on_purchase_done(result: Dictionary) -> void :
	if result.get("ok", false):
		_ask(false)
	else:
		hud.show_gate(_gate_mode, result.get("state", {}), str(result.get("message", "Не получилось")))


func _launch(countdown: float) -> void :
	started = true
	if countdown > 0.0:
		counting_down = true
		_countdown_left = countdown
		hud.show_countdown(ceili(countdown))
	else:
		_begin()


func _begin() -> void :
	counting_down = false
	hud.show_countdown(0)
	hud.show_hint()
	player.running = true



func restart() -> void :
	var fresh: Node3D = load(scene_file_path).instantiate()
	fresh.spawn_entities = spawn_entities
	fresh.level_seed = level_seed
	fresh.wait_for_start = false
	fresh.countdown_without_card = true
	fresh.bridge = bridge
	var parent: = get_parent()
	var was_current: = get_tree().current_scene == self
	name = "RunOld"
	get_tree().paused = false
	parent.add_child(fresh)
	if was_current:
		get_tree().current_scene = fresh
	queue_free()



func play_cutscene() -> void :
	if intro or started:
		return
	hud.show_start(false)
	hud.visible = false
	intro = Intro.new()
	intro.line_started.connect( func(_i: int, g: StringName) -> void : player.rig.gesture(g))
	intro.finished.connect(_on_intro_finished)
	player.rig.drunk = true
	_turn_rig(deg_to_rad(INTRO_TURN_DEG))
	add_child(intro)


func _on_intro_finished(_skipped: bool) -> void :
	intro = null
	player.rig.drunk = false
	player.rig.talking = false
	_turn_rig(player.TURN_TO_CAMERA)
	music.volume_db = _music_db
	hud.visible = true
	hud.show_start(true)


func _turn_rig(to: float) -> void :
	create_tween().set_trans(Tween.TRANS_SINE).tween_property(player.rig, "rotation:y", to, 0.6)


func _on_game_over() -> void :
	hud.show_game_over()


func _on_damaged(_lives: int) -> void :
	player.set_invulnerable(RunRules.INVULNERABLE)
	sfx_hit.play()
	hud.flash_hit()
	_shake = 0.35


func _physics_process(delta: float) -> void :
	if not started:
		if intro:
			player.rig.talking = intro.is_typing()

			music.volume_db = _music_db - (10.0 if intro.voice.playing else 0.0)
		elif intro_blend < 0.01 and not _asking and not hud.gate_panel.visible and Input.is_action_just_pressed("jump"):
			start(COUNTDOWN)
		_follow_camera(delta)
		return
	if counting_down:
		_countdown_left -= delta
		hud.show_countdown(ceili(_countdown_left))
		if _countdown_left <= 0.0:
			_begin()
		_follow_camera(delta)
		return
	if state.is_over:
		return
	time += delta
	state.tick(delta)
	player.speed = RunRules.speed_at(time)
	_fill()
	_cleanup()
	_follow_camera(delta)
	_check_stall(delta)
	_update_warnings()

	if player.global_position.x > _best_x:
		state.add_distance(player.global_position.x - _best_x)
		_best_x = player.global_position.x


func _process(delta: float) -> void :
	_watch_fps(delta)


func _fill() -> void :
	while generator.end_x < player.global_position.x + AHEAD:
		var from_x: = generator.end_x
		var chunk: = generator.next_chunk(maxf(player.global_position.x, 0.0))
		for p in chunk.platforms:
			PlatformBuilder.build(p, level)
			platforms.append(p)
		if spawn_entities:
			_spawn(chunk.spawns)
		Decor.populate(_decor_rng, from_x, generator.end_x, decor)


func _spawn(spawns: Array) -> void :
	for s in spawns:
		var e: = spawn_entity(s.kind, s.pos)
		e.hops = s.get("hop", false)


func spawn_entity(kind: StringName, pos: Vector2) -> RunEntity:
	var e: = EntityFactory.spawn(kind, pos, entities, player)
	e.touched_player.connect(_on_entity_touched)
	return e


func _on_entity_touched(e: RunEntity, body: CharacterBody3D) -> void :
	if e.is_queued_for_deletion() or state.is_over:
		return
	match e.kind:
		&"gift":
			state.award(&"gift")
			sfx_gift.play()
			e.smash()
		&"crate":
			if e.is_stomp_from(body):
				_break_crate(e)
				player.bounce()
		&"snowman":
			if e.is_stomp_from(body):
				state.award(&"snowman_stomp")
				sfx_kill.play()
				e.smash()
				player.bounce()
			else:
				_hurt()
		&"crow", &"spikes":
			_hurt()


func _break_crate(e: RunEntity, award: = true) -> void :
	if e.is_queued_for_deletion():
		return
	if award:
		state.award(&"crate")
	var at: = Vector2(e.position.x, e.position.y + 0.8)

	for other in entities.get_children():
		if other is RunEntity and other != e and other.kind == &"crate"\
		and absf(other.position.x - e.position.x) < 0.2 and other.position.y > e.position.y + 0.5:
			other.base.y -= 1.0
			create_tween().tween_property(other, "position:y", other.base.y, 0.25)\
			.set_trans(Tween.TRANS_QUAD).set_ease(Tween.EASE_IN)
	e.smash()
	if award:
		spawn_entity.call_deferred(&"gift", at)



func crate_ahead() -> RunEntity:
	var p: = player.global_position
	for e in entities.get_children():
		if e is RunEntity and e.kind == &"crate" and not e.is_queued_for_deletion():
			var dx: float = e.position.x - p.x
			if dx > 0.0 and dx < 1.2 and e.position.y < p.y + 1.4 and e.position.y + 1.0 > p.y + 0.05:
				return e
	return null


func _hurt() -> void :
	if not player.is_invulnerable:
		state.take_hit(time)


func _on_throw_requested(from: Vector3) -> void :
	if not player.running:
		return
	var ball: = Snowball.create(from, player.speed)
	ball.set_meta("end_x", INF)
	ball.hit.connect(_on_snowball_hit)
	entities.add_child(ball)


func _on_snowball_hit(target: RunEntity) -> void :
	match target.kind:
		&"crate":
			_break_crate(target)
		&"snowman", &"crow":
			state.award(target.kind)
			sfx_kill.play()
			target.smash()



func _check_stall(delta: float) -> void :
	var x: = player.global_position.x
	if player.is_on_wall() and x - _stall_x < 0.05:
		_stall_time += delta
	else:
		_stall_time = 0.0
		_stall_x = x
	if _stall_time >= STALL_TIME:
		_stall_time = 0.0
		state.take_hit(time)

		var crate: = crate_ahead()
		if crate:
			_break_crate(crate, false)
		else:
			player.respawn(respawn_point(x + 0.5))



func _update_warnings() -> void :
	var right: = camera.global_position.x + _half_view_width()
	var ys: Array[float] = []
	for e in entities.get_children():
		if e is RunEntity and e.kind == &"crow" and not e.is_queued_for_deletion():
			var ahead: float = e.global_position.x - right
			if ahead > 0.0 and ahead < WARN_RANGE:
				var screen: = camera.unproject_position(Vector3(right - 1.0, e.global_position.y, RunEntity.FRONT_Z))
				ys.append(screen.y)
	hud.set_warnings(ys)


func _half_view_width() -> float:
	var vp: = get_viewport().get_visible_rect().size
	var depth: = CAMERA_OFFSET.z - RunEntity.FRONT_Z
	return depth * tan(deg_to_rad(camera.fov * 0.5)) * (vp.x / vp.y)


func _cleanup() -> void :
	var limit: = camera.global_position.x - BEHIND
	for parent in [level, entities]:
		for child in parent.get_children():
			if child.get_meta("end_x", INF) < limit:
				child.queue_free()
	for child in decor.get_children():

		if child.get_meta("end_x", INF) < limit - 25.0:
			child.queue_free()
	while not platforms.is_empty() and platforms[0].end.x < limit:
		platforms.pop_front()


func _follow_camera(delta: float) -> void :
	if player.is_on_floor():
		_ground_y = player.global_position.y
	var target: = player.global_position + CAMERA_OFFSET

	target.y = maxf(target.y, _ground_y - 1.0 + CAMERA_OFFSET.y)
	var y: = lerpf(camera.global_position.y, target.y, 1.0 - exp( - CAMERA_Y_SMOOTH * delta))
	var shake: = Vector3.ZERO
	if _shake > 0.0:
		_shake = maxf(_shake - delta, 0.0)
		shake = Vector3(randf_range(-1, 1), randf_range(-1, 1), 0) * _shake * 0.6
	var base_pos: = Vector3(target.x, y, CAMERA_OFFSET.z)



	var want: = 1.0 if _is_blocked_by_step() else 0.0
	reveal = move_toward(reveal, want, delta / (REVEAL_IN if want > reveal else REVEAL_OUT))
	var r: = smoothstep(0.0, 1.0, reveal)
	var focus: = player.global_position + Vector3(2.0, 1.0, RunEntity.FRONT_Z)
	var reveal_pos: = player.global_position + REVEAL_OFFSET
	var pos: = base_pos.lerp(reveal_pos, r)
	var base_basis: = Basis.from_euler(Vector3(deg_to_rad(BASE_PITCH_DEG), 0.0, 0.0))
	var basis: = base_basis.slerp(Basis.looking_at(focus - reveal_pos, Vector3.UP), r)


	intro_blend = move_toward(intro_blend, 1.0 if intro else 0.0, delta / INTRO_EASE)
	var ib: = smoothstep(0.0, 1.0, intro_blend)
	if ib > 0.0:
		var chest: = player.global_position + Vector3(0.0, 1.0, RunEntity.FRONT_Z)
		var drift: = Time.get_ticks_msec() / 1000.0
		var close: = chest + INTRO_CAMERA + Vector3(0.25 * sin(drift * 0.35), 0.08 * sin(drift * 0.5), 0.0)
		pos = pos.lerp(close, ib)
		basis = basis.slerp(Basis.looking_at(chest + Vector3(1.0, -0.6, 0.0) - close, Vector3.UP), ib)
	camera.global_transform = Transform3D(basis, pos + shake)
	field.global_position.x = target.x



func _is_blocked_by_step() -> bool:
	if not player.is_on_wall() or not player.running:
		return false
	var x: = player.global_position.x
	for p in platforms:
		if p.position.x > x - 0.5 and p.position.x < x + 1.5:
			return p.position.y > player.global_position.y + 0.4
	return false


func _on_fell_into_pit() -> void :
	player.respawn(respawn_point(player.global_position.x))
	state.take_hit(time)
	player_fell.emit()



func respawn_point(x: float) -> Vector3:
	for p in platforms:
		if p.end.x > x + 1.5:
			var rx: = clampf(x, p.position.x + 1.0, p.end.x - 1.0)
			return Vector3(rx, p.position.y + 0.1, 0.0)
	return Vector3(x, 2.0, 0.0)



func _watch_fps(delta: float) -> void :
	_fps_frames += 1
	_fps_time += delta
	if _fps_time < 2.0:
		return
	var fps: = _fps_frames / _fps_time
	_fps_frames = 0
	_fps_time = 0.0
	_fps_low = _fps_low + 1 if fps < 30.0 else 0
	if _fps_low >= 3 and sun.shadow_enabled and DisplayServer.is_touchscreen_available():
		sun.shadow_enabled = false
