extends CanvasLayer



signal start_requested()
signal retry_requested()
signal intro_requested()

signal gate_choice(choice: String, count: int)

const ACCENT: = Color(0.984, 0.278, 0.294)
const HUD_AMBER: = Color(1.0, 0.706, 0.227)
const MUTED: = Color(0.72, 0.74, 0.8)
const CHIP: = Color(0.094, 0.102, 0.129, 0.72)
const VIEW: = Vector2(1800, 893)

const HEART: = preload("res://assets/ui/heart.png")
const HEART_EMPTY: = preload("res://assets/ui/heart_empty.png")
const GIFT: = preload("res://assets/ui/gift.png")
const LEADERBOARD_ITEM: = preload("res://LeaderBoardItem.tscn")

var score_label: Label
var combo_label: Label
var gifts_label: Label
var distance_label: Label
var hearts: Array[TextureRect] = []
var pause_button: Button
var start_panel: Control
var pause_panel: Control
var game_over_panel: Control
var status_label: Label
var result_score: Label
var result_stats: Label
var board: VBoxContainer
var retry_button: Button
var countdown_label: Label
var hint_label: Label
var hit_flash: ColorRect
var warn_layer: Control
var _warn_markers: Array[Label] = []

var _state: RunState

var bridge: Node = SiteBridge
var gate_panel: Control
var gate_title: Label
var gate_text: Label
var gate_list: VBoxContainer
var gate_back: Button
var gate_confirm: VBoxContainer
var gate_confirm_text: Label
var gate_confirm_yes: Button

var gate_buttons: = {}
var _confirm_action: Array = []


func _ready() -> void :
	process_mode = Node.PROCESS_MODE_ALWAYS
	var root: = Control.new()
	root.set_anchors_preset(Control.PRESET_FULL_RECT)
	root.mouse_filter = Control.MOUSE_FILTER_IGNORE
	add_child(root)
	_build_top_bar(root)
	_build_touch(root)
	_build_effects(root)
	start_panel = _build_start(root)
	pause_panel = _build_pause(root)
	game_over_panel = _build_game_over(root)
	gate_panel = _build_gate(root)
	pause_panel.hide()
	game_over_panel.hide()
	gate_panel.hide()
	bridge.score_saved.connect(_on_score_saved)
	bridge.top_loaded.connect(_on_top_loaded)


func bind(state: RunState) -> void :
	_state = state
	state.changed.connect(_refresh)
	_refresh()


func lives_left() -> int:
	var n: = 0
	for h in hearts:
		if h.texture == HEART:
			n += 1
	return n


func _refresh() -> void :
	score_label.text = str(_state.score)
	combo_label.text = "×%d" % _state.multiplier
	combo_label.visible = _state.multiplier > 1
	gifts_label.text = str(_state.gifts)
	distance_label.text = "%d м" % int(_state.distance)
	for i in hearts.size():
		hearts[i].texture = HEART if i < _state.lives else HEART_EMPTY




func show_countdown(n: int) -> void :
	countdown_label.visible = n > 0
	if n > 0 and countdown_label.text != str(n):
		countdown_label.text = str(n)
		countdown_label.scale = Vector2(1.4, 1.4)
		create_tween().tween_property(countdown_label, "scale", Vector2.ONE, 0.25)



func show_hint() -> void :
	hint_label.text = "Справа — прыжок (ещё раз в воздухе — двойной), слева — снежок" if DisplayServer.is_touchscreen_available() else "Пробел — прыжок, ещё раз в воздухе — двойной   •   X / J / клик — снежок   •   Esc — пауза"
	hint_label.modulate.a = 1.0
	hint_label.show()
	var t: = create_tween()
	t.tween_interval(5.0)
	t.tween_property(hint_label, "modulate:a", 0.0, 0.8)
	t.tween_callback(hint_label.hide)


func flash_hit() -> void :
	hit_flash.color.a = 0.45
	create_tween().tween_property(hit_flash, "color:a", 0.0, 0.45)



func set_warnings(ys: Array[float]) -> void :
	while _warn_markers.size() < ys.size():
		var m: = _label(warn_layer, "!", 64, ACCENT)
		m.add_theme_constant_override("outline_size", 14)
		_warn_markers.append(m)
	for i in _warn_markers.size():
		var m: = _warn_markers[i]
		m.visible = i < ys.size()
		if m.visible:
			m.position = Vector2(VIEW.x - 70, clampf(ys[i] - 40, 110, VIEW.y - 120))
			m.modulate.a = 0.55 + 0.45 * absf(sin(Time.get_ticks_msec() / 120.0))




func show_start(visible_now: bool) -> void :
	start_panel.visible = visible_now


func toggle_pause() -> void :
	if game_over_panel.visible or start_panel.visible or gate_panel.visible or not visible:
		return
	var paused: = not get_tree().paused
	get_tree().paused = paused
	pause_panel.visible = paused


func _unhandled_input(event: InputEvent) -> void :
	if event.is_action_pressed("pause"):
		toggle_pause()


func _notification(what: int) -> void :

	if what == NOTIFICATION_APPLICATION_FOCUS_OUT and not get_tree().paused:
		toggle_pause()


func show_game_over() -> void :
	get_tree().paused = true
	pause_panel.hide()
	var p: = _state.to_payload()
	result_score.text = "Очки: %d" % p.score
	result_stats.text = "%d м   •   подарки %d   •   ящики %d   •   враги %d   •   %d:%02d" % [
		p.distance, p.gifts, p.crates, p.enemies, p.duration / 60, p.duration % 60]
	_clear_board()
	game_over_panel.modulate.a = 0.0
	game_over_panel.show()
	create_tween().tween_property(game_over_panel, "modulate:a", 1.0, 0.4)
	retry_button.grab_focus()

	if not bridge.is_available():
		_set_status("Результат не сохранён: игра открыта не на сайте", MUTED)
		return
	if bridge.is_logged_in():
		_set_status("Сохраняю результат…", MUTED)
		bridge.submit_run(p)
	else:
		_set_status("Войдите на сайт, чтобы результат попал в таблицу", MUTED)
		bridge.fetch_top()


func _set_status(text: String, color: Color) -> void :
	status_label.text = text
	status_label.add_theme_color_override("font_color", color)


func _on_score_saved(result: Dictionary) -> void :
	if result.get("ok", false):
		var text: = "Результат сохранён! Лучший: %d" % int(result.get("best", 0))
		if int(result.get("rank", 0)) > 0:
			text += ", место #%d" % int(result.get("rank"))

		if int(result.get("xp_gained", 0)) > 0:
			text += "\n+%d XP батлпасса" % int(result.get("xp_gained"))
			if int(result.get("level_after", 0)) > int(result.get("level_before", 0)):
				text += "  •  Уровень %d! Награда ждёт в батлпассе" % int(result.get("level_after"))
		elif str(result.get("mode", "")) == "record":
			text += "\nЗабег без опыта: лимит на сегодня исчерпан"
		_set_status(text, ACCENT)
	else:
		_set_status(str(result.get("message", "Не удалось сохранить результат")), MUTED)
	bridge.fetch_top()


func _on_top_loaded(result: Dictionary) -> void :
	if not game_over_panel.visible:
		return
	_clear_board()
	var entries: Variant = result.get("entries", [])
	var me: String = bridge.user_login()
	var index: = 1
	for data in (entries if entries is Array else []):
		if data is Dictionary:
			var item: LeaderBoardItem = LEADERBOARD_ITEM.instantiate()
			var entry: = LeaderBoardModel.from_dict(data)
			item.set_entry(index, entry, me != "" and entry.name == me)
			board.add_child(item)
			index += 1
			if index > 5:
				break


func _clear_board() -> void :
	for child in board.get_children():
		child.queue_free()




func _chip(parent: Control) -> HBoxContainer:
	var panel: = PanelContainer.new()
	var sb: = StyleBoxFlat.new()
	sb.bg_color = CHIP
	sb.set_corner_radius_all(16)
	sb.content_margin_left = 18
	sb.content_margin_right = 20
	sb.content_margin_top = 6
	sb.content_margin_bottom = 6
	panel.add_theme_stylebox_override("panel", sb)
	panel.mouse_filter = Control.MOUSE_FILTER_IGNORE
	parent.add_child(panel)
	var row: = HBoxContainer.new()
	row.add_theme_constant_override("separation", 10)
	row.alignment = BoxContainer.ALIGNMENT_CENTER
	panel.add_child(row)
	return row


func _label(parent: Control, text: String, size: int, color: = Color.WHITE) -> Label:
	var l: = Label.new()
	l.text = text
	l.add_theme_font_size_override("font_size", size)
	l.add_theme_color_override("font_color", color)
	parent.add_child(l)
	return l


func _icon(parent: Control, tex: Texture2D, size: float) -> TextureRect:
	var t: = TextureRect.new()
	t.texture = tex
	t.expand_mode = TextureRect.EXPAND_IGNORE_SIZE
	t.stretch_mode = TextureRect.STRETCH_KEEP_ASPECT_CENTERED
	t.custom_minimum_size = Vector2(size, size)
	parent.add_child(t)
	return t


func _build_top_bar(root: Control) -> void :
	var bar: = HBoxContainer.new()
	bar.set_anchors_preset(Control.PRESET_TOP_WIDE)
	bar.offset_left = 28
	bar.offset_right = -28
	bar.offset_top = 22
	bar.add_theme_constant_override("separation", 16)
	bar.mouse_filter = Control.MOUSE_FILTER_IGNORE
	root.add_child(bar)

	var score_chip: = _chip(bar)
	score_label = _label(score_chip, "0", 52, HUD_AMBER)
	combo_label = _label(score_chip, "×2", 34, ACCENT)

	var spacer: = Control.new()
	spacer.size_flags_horizontal = Control.SIZE_EXPAND_FILL
	spacer.mouse_filter = Control.MOUSE_FILTER_IGNORE
	bar.add_child(spacer)

	var gifts_chip: = _chip(bar)
	_icon(gifts_chip, GIFT, 44)
	gifts_label = _label(gifts_chip, "0", 38)

	var dist_chip: = _chip(bar)
	distance_label = _label(dist_chip, "0 м", 38)

	var hearts_chip: = _chip(bar)
	for i in RunRules.LIVES:
		hearts.append(_icon(hearts_chip, HEART, 42))

	pause_button = Button.new()
	pause_button.text = "II"
	pause_button.add_theme_font_size_override("font_size", 40)
	pause_button.custom_minimum_size = Vector2(96, 96) if DisplayServer.is_touchscreen_available() else Vector2(76, 76)
	pause_button.focus_mode = Control.FOCUS_NONE
	pause_button.pressed.connect(toggle_pause)
	bar.add_child(pause_button)


func _build_effects(root: Control) -> void :
	hit_flash = ColorRect.new()
	hit_flash.set_anchors_preset(Control.PRESET_FULL_RECT)
	hit_flash.color = Color(ACCENT, 0.0)
	hit_flash.mouse_filter = Control.MOUSE_FILTER_IGNORE
	root.add_child(hit_flash)

	warn_layer = Control.new()
	warn_layer.set_anchors_preset(Control.PRESET_FULL_RECT)
	warn_layer.mouse_filter = Control.MOUSE_FILTER_IGNORE
	root.add_child(warn_layer)

	countdown_label = _label(root, "3", 200, Color.WHITE)
	countdown_label.add_theme_constant_override("outline_size", 24)
	countdown_label.horizontal_alignment = HORIZONTAL_ALIGNMENT_CENTER
	countdown_label.vertical_alignment = VERTICAL_ALIGNMENT_CENTER
	countdown_label.size = Vector2(400, 300)
	countdown_label.position = VIEW * 0.5 - countdown_label.size * 0.5
	countdown_label.pivot_offset = countdown_label.size * 0.5
	countdown_label.hide()

	hint_label = _label(root, "", 30, Color.WHITE)
	hint_label.horizontal_alignment = HORIZONTAL_ALIGNMENT_CENTER
	hint_label.size = Vector2(VIEW.x, 60)
	hint_label.position = Vector2(0, 140)
	hint_label.hide()


func _build_touch(root: Control) -> void :
	var layer: = Node2D.new()
	root.add_child(layer)
	for spec in [["throw", Vector2(48, VIEW.y - 240), "btn_throw"], ["jump", Vector2(VIEW.x - 240, VIEW.y - 240), "btn_jump"]]:
		var b: = TouchScreenButton.new()
		b.action = spec[0]
		b.position = spec[1]
		b.texture_normal = load("res://assets/ui/%s.png" % spec[2])
		b.texture_pressed = load("res://assets/ui/%s_pressed.png" % spec[2])
		b.visibility_mode = TouchScreenButton.VISIBILITY_TOUCHSCREEN_ONLY
		layer.add_child(b)



func _overlay(root: Control, min_width: float) -> Array:
	var layer: = Control.new()
	layer.set_anchors_preset(Control.PRESET_FULL_RECT)
	root.add_child(layer)
	var dim: = ColorRect.new()
	dim.set_anchors_preset(Control.PRESET_FULL_RECT)
	dim.color = Color(0, 0, 0, 0.55)
	layer.add_child(dim)
	var center: = CenterContainer.new()
	center.set_anchors_preset(Control.PRESET_FULL_RECT)
	layer.add_child(center)
	var card: = PanelContainer.new()
	card.custom_minimum_size = Vector2(min_width, 0)
	center.add_child(card)
	var box: = VBoxContainer.new()
	box.add_theme_constant_override("separation", 14)
	card.add_child(box)
	return [layer, box]


func _centered(l: Label) -> Label:
	l.horizontal_alignment = HORIZONTAL_ALIGNMENT_CENTER
	l.add_theme_constant_override("outline_size", 0)
	return l


func _build_start(root: Control) -> Control:
	var o: = _overlay(root, 900)
	var box: VBoxContainer = o[1]
	_centered(_label(box, "Санта бежит за подарками", 54))
	var touch: = DisplayServer.is_touchscreen_available()
	var how: = "Кнопка справа — прыжок, нажми ещё раз в воздухе — двойной.\nКнопка слева — снежок: разбивает ящики, сбивает снеговиков и ворон." if touch else"""Пробел / ↑ — прыжок, ещё раз в воздухе — двойной.
X / J или клик мышью по игре — снежок: разбивает ящики, сбивает снеговиков и ворон."""
	var l: = _centered(_label(box, how, 28, MUTED))
	l.autowrap_mode = TextServer.AUTOWRAP_WORD_SMART
	_centered(_label(box, "Прыгай на снеговиков сверху. Шипы, вороны и упор в стену отнимают жизнь.\nКрасный «!» справа — летит ворона.", 26, MUTED))
	var go: = Button.new()
	go.text = "Начать забег"
	go.size_flags_horizontal = Control.SIZE_SHRINK_CENTER
	go.pressed.connect( func() -> void : start_requested.emit())
	box.add_child(go)
	var story: = Button.new()
	story.text = "Как всё было"
	story.flat = true
	story.size_flags_horizontal = Control.SIZE_SHRINK_CENTER
	story.pressed.connect( func() -> void : intro_requested.emit())
	box.add_child(story)
	return o[0]


func _build_pause(root: Control) -> Control:
	var o: = _overlay(root, 520)
	var box: VBoxContainer = o[1]
	_centered(_label(box, "Пауза", 54))
	var resume: = Button.new()
	resume.text = "Продолжить"
	resume.size_flags_horizontal = Control.SIZE_SHRINK_CENTER
	resume.pressed.connect(toggle_pause)
	box.add_child(resume)
	return o[0]


func _build_game_over(root: Control) -> Control:
	var o: = _overlay(root, 1100)
	var box: VBoxContainer = o[1]
	_centered(_label(box, "Забег окончен", 56, ACCENT))
	result_score = _centered(_label(box, "Очки: 0", 48, HUD_AMBER))
	result_stats = _centered(_label(box, "", 26, MUTED))
	status_label = _centered(_label(box, "", 26))
	status_label.autowrap_mode = TextServer.AUTOWRAP_WORD_SMART
	_centered(_label(box, "Рекорды сервера", 30))
	board = VBoxContainer.new()
	board.add_theme_constant_override("separation", 8)
	board.custom_minimum_size = Vector2(0, 120)
	box.add_child(board)
	retry_button = Button.new()
	retry_button.text = "Ещё забег"
	retry_button.size_flags_horizontal = Control.SIZE_SHRINK_CENTER
	retry_button.pressed.connect( func() -> void : retry_requested.emit())
	box.add_child(retry_button)
	return o[0]





func show_gate_wait() -> void :
	_fill_gate("Минутку…", "Связываюсь с сайтом", [])


func hide_gate() -> void :
	gate_panel.hide()




func show_gate(mode: String, state: Dictionary, message: = "") -> void :
	var coins: = int(state.get("coins", 0))
	var ticket_coins: = int(state.get("ticket_coins", 80))
	var title: = "Не удалось начать забег"
	var text: = message
	var buttons: Array = []
	match mode:
		"no_tickets":
			title = "Нет билетов"
			text = ("Забег стоит 1 билет. Заработай монеты на сервере вечером: 1 монета за минуту игры "
				+ "и бонусы за серии убийств без смерти. Билет — %d монет, у тебя %d." % [ticket_coins, coins])
			if coins >= ticket_coins:
				buttons.append(["exchange", 1, "Обменять %d монет на билет" % ticket_coins, ""])
			for pack in state.get("packs", []):
				buttons.append(["tickets", int(pack[0]), "%d %s — %s ₽" % [int(pack[0]), _tickets_word(int(pack[0])), _money(pack[1])], _money(pack[1])])
		"limit_reached":
			title = "Лимит опыта на сегодня исчерпан"
			text = "Завтра снова будут забеги с опытом. Можно расширить лимит или сыграть только на рекорд: билет спишется, опыта не будет."
			if int(state.get("extra_left", 0)) > 0:
				buttons.append(["extend", 1, "+%d забега с опытом — %s ₽" % [int(state.get("ext_runs", 2)), _money(state.get("ext_price", 0))], _money(state.get("ext_price", 0))])
			buttons.append(["no_xp", 0, "Играть без опыта (1 билет)", ""])
		"login":
			title = "Войдите на сайт, чтобы играть"
			text = "Забег стоит билет, а билеты, опыт и награды батлпасса привязаны к аккаунту."
		"season_closed":
			title = "Сезон батлпасса сейчас не идёт"
			text = "Забеги откроются вместе с новым сезоном."
	if message != "" and mode in ["no_tickets", "limit_reached"]:
		text = message + "\n\n" + text
	_fill_gate(title, text, buttons)


func _fill_gate(title: String, text: String, buttons: Array) -> void :
	gate_title.text = title
	gate_text.text = text
	gate_confirm.hide()
	gate_buttons.clear()
	for child in gate_list.get_children():
		child.queue_free()
	for spec in buttons:
		var b: = Button.new()
		b.text = spec[2]
		b.size_flags_horizontal = Control.SIZE_SHRINK_CENTER
		var choice: String = spec[0]
		var count: int = spec[1]
		var price: String = spec[3]
		b.pressed.connect( func() -> void : _on_gate_button(choice, count, price))
		gate_list.add_child(b)
		gate_buttons[choice + (":%d" % count if choice == "tickets" else "")] = b
	gate_back.visible = not buttons.is_empty() or title != "Минутку…"
	gate_panel.show()



func _on_gate_button(choice: String, count: int, price: String) -> void :
	if price == "":
		gate_choice.emit(choice, count)
		return
	gate_confirm_text.text = "Списать %s ₽ с баланса сайта?" % price
	_confirm_action = [choice, count]
	gate_confirm.show()


func _tickets_word(n: int) -> String:
	if n % 10 == 1 and n % 100 != 11:
		return "билет"
	if n % 10 >= 2 and n % 10 <= 4 and (n % 100 < 12 or n % 100 > 14):
		return "билета"
	return "билетов"


func _money(value: Variant) -> String:
	var v: = float(value)
	return str(int(v)) if is_equal_approx(v, roundf(v)) else "%.2f" % v


func _build_gate(root: Control) -> Control:
	var o: = _overlay(root, 900)
	var box: VBoxContainer = o[1]
	gate_title = _centered(_label(box, "", 48))
	gate_text = _centered(_label(box, "", 26, MUTED))
	gate_text.autowrap_mode = TextServer.AUTOWRAP_WORD_SMART
	gate_list = VBoxContainer.new()
	gate_list.add_theme_constant_override("separation", 10)
	box.add_child(gate_list)
	gate_confirm = VBoxContainer.new()
	gate_confirm.add_theme_constant_override("separation", 10)
	box.add_child(gate_confirm)
	gate_confirm_text = _centered(_label(gate_confirm, "", 30, HUD_AMBER))
	var row: = HBoxContainer.new()
	row.alignment = BoxContainer.ALIGNMENT_CENTER
	row.add_theme_constant_override("separation", 14)
	gate_confirm.add_child(row)
	gate_confirm_yes = Button.new()
	gate_confirm_yes.text = "Да, купить"
	gate_confirm_yes.pressed.connect( func() -> void :
		gate_confirm.hide()
		gate_choice.emit(_confirm_action[0], _confirm_action[1]))
	row.add_child(gate_confirm_yes)
	var no: = Button.new()
	no.text = "Нет"
	no.pressed.connect(gate_confirm.hide)
	row.add_child(no)
	gate_back = Button.new()
	gate_back.text = "Назад"
	gate_back.flat = true
	gate_back.size_flags_horizontal = Control.SIZE_SHRINK_CENTER
	gate_back.pressed.connect( func() -> void : gate_choice.emit("close", 0))
	box.add_child(gate_back)
	return o[0]
