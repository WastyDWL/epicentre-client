class_name Intro
extends CanvasLayer








signal line_started(index: int, gesture: StringName)
signal finished(skipped: bool)

const SPEAKER: = "Санта"
const ACCENT: = Color(0.984, 0.278, 0.294)
const MUTED: = Color(0.72, 0.74, 0.8)
const CHARS_PER_SEC: = 40.0
const VOICE_DIR: = "res://assets/voice/"
const LINES: = [
	{"text": "Ик!.. О, привет! Ты случайно не видел мои сани? И подарки… особенно подарки.", "gesture": &"hic"}, 
	{"text": "Понимаешь, вчера эльфы устроили корпоратив. Я сказал: «Только одну рюмочку — за Рождество!»", "gesture": &"drink"}, 
	{"text": "Потом была ещё одна. И ещё. А потом олени сказали, что я пою лучше Синатры…", "gesture": &"shrug"}, 
	{"text": "Утром просыпаюсь — мешок пустой. Все подарки растерял по дороге, до самого Полюса!", "gesture": &"hic"}, 
	{"text": "Снеговики их уже прибирают к рукам, вороны кружат, а часть я сам зачем-то запер в ледяные ящики.", "gesture": &"point"}, 
	{"text": "Поможешь собрать? Беги, прыгай, кидай снежки! А я… ик… подержу бутылочку. Для храбрости.", "gesture": &"cheer"}, 
]


static var save_path: = "user://intro.cfg"

var index: = -1
var text_label: RichTextLabel
var hint_label: Label
var skip_button: Button
var voice: AudioStreamPlayer
var _root: Control
var _shown: = 0.0
var _done: = false


static func was_seen() -> bool:
	var cfg: = ConfigFile.new()
	return cfg.load(save_path) == OK and cfg.get_value("intro", "seen", false)


static func mark_seen() -> void :
	var cfg: = ConfigFile.new()
	cfg.set_value("intro", "seen", true)
	cfg.save(save_path)


static func voice_for(i: int) -> AudioStream:
	for ext in ["ogg", "mp3", "wav"]:
		var path: = VOICE_DIR + "intro_%02d.%s" % [i + 1, ext]
		if ResourceLoader.exists(path):
			return load(path)
	return null


func _ready() -> void :
	layer = 5
	process_mode = Node.PROCESS_MODE_ALWAYS
	voice = AudioStreamPlayer.new()
	add_child(voice)
	_build()
	next()


func is_typing() -> bool:
	return index >= 0 and text_label.visible_ratio < 1.0



func next() -> void :
	if _done:
		return
	if is_typing():
		text_label.visible_ratio = 1.0
		return
	index += 1
	if index >= LINES.size():
		_finish(false)
		return
	var line: Dictionary = LINES[index]
	text_label.text = line.text
	text_label.visible_ratio = 0.0
	_shown = 0.0
	hint_label.text = "Дальше — клик или пробел" if index < LINES.size() - 1 else "Начать — клик или пробел"
	voice.stop()
	var stream: = voice_for(index)
	if stream:
		voice.stream = stream
		voice.play()
	line_started.emit(index, line.get("gesture", &""))


func skip() -> void :
	_finish(true)


func _finish(skipped: bool) -> void :
	if _done:
		return
	_done = true
	voice.stop()
	mark_seen()
	finished.emit(skipped)
	var t: = create_tween()
	t.tween_property(_root, "modulate:a", 0.0, 0.3)
	t.tween_callback(queue_free)


func _process(delta: float) -> void :
	if index < 0 or _done or text_label.visible_ratio >= 1.0:
		return
	_shown += delta * CHARS_PER_SEC
	var total: = text_label.get_total_character_count()
	text_label.visible_ratio = 1.0 if total == 0 else minf(_shown / total, 1.0)


func _unhandled_input(event: InputEvent) -> void :
	if _done:
		return
	if event.is_action_pressed("pause"):
		skip()
	elif event.is_action_pressed("jump") or event.is_action_pressed("ui_accept")\
	or (event is InputEventMouseButton and event.pressed and event.button_index == MOUSE_BUTTON_LEFT)\
	or (event is InputEventScreenTouch and event.pressed):
		next()
	else:
		return
	get_viewport().set_input_as_handled()


func _build() -> void :
	var root: = Control.new()
	_root = root
	root.set_anchors_preset(Control.PRESET_FULL_RECT)
	root.mouse_filter = Control.MOUSE_FILTER_IGNORE
	add_child(root)


	for top in [true, false]:
		var bar: = ColorRect.new()
		bar.color = Color(0, 0, 0, 0.85)
		bar.mouse_filter = Control.MOUSE_FILTER_IGNORE
		bar.set_anchors_preset(Control.PRESET_TOP_WIDE if top else Control.PRESET_BOTTOM_WIDE)
		bar.custom_minimum_size = Vector2(0, 70)
		if not top:
			bar.offset_top = -70
		root.add_child(bar)

	var card: = PanelContainer.new()
	var sb: = StyleBoxFlat.new()
	sb.bg_color = Color(0.07, 0.075, 0.1, 0.92)
	sb.border_color = Color(ACCENT, 0.9)
	sb.border_width_left = 6
	sb.set_corner_radius_all(18)
	sb.set_content_margin_all(26)
	sb.content_margin_left = 34
	card.add_theme_stylebox_override("panel", sb)
	card.set_anchors_preset(Control.PRESET_BOTTOM_WIDE)
	card.offset_left = 160
	card.offset_right = -160
	card.offset_top = -330
	card.offset_bottom = -100
	card.mouse_filter = Control.MOUSE_FILTER_IGNORE
	root.add_child(card)
	var box: = VBoxContainer.new()
	box.add_theme_constant_override("separation", 10)
	card.add_child(box)

	var name_label: = Label.new()
	name_label.text = SPEAKER
	name_label.add_theme_font_size_override("font_size", 34)
	name_label.add_theme_color_override("font_color", ACCENT)
	box.add_child(name_label)

	text_label = RichTextLabel.new()
	text_label.bbcode_enabled = false
	text_label.fit_content = true
	text_label.scroll_active = false
	text_label.size_flags_vertical = Control.SIZE_EXPAND_FILL
	text_label.mouse_filter = Control.MOUSE_FILTER_IGNORE
	text_label.add_theme_font_size_override("normal_font_size", 36)
	text_label.add_theme_color_override("default_color", Color.WHITE)
	box.add_child(text_label)

	hint_label = Label.new()
	hint_label.add_theme_font_size_override("font_size", 24)
	hint_label.add_theme_color_override("font_color", MUTED)
	hint_label.horizontal_alignment = HORIZONTAL_ALIGNMENT_RIGHT
	box.add_child(hint_label)

	skip_button = Button.new()
	skip_button.text = "Пропустить  »"
	skip_button.focus_mode = Control.FOCUS_NONE
	skip_button.set_anchors_preset(Control.PRESET_TOP_RIGHT)
	skip_button.offset_left = -330
	skip_button.offset_right = -40
	skip_button.offset_top = 90
	skip_button.offset_bottom = 170
	skip_button.pressed.connect(skip)
	root.add_child(skip_button)
