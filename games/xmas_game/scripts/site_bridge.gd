extends Node

# Мост игры к сайту epicentre-cs.ru. Два режима:
#  - веб (страница /xmas_game): window.XmasBridge страницы, сессия сайта;
#  - клиент «Эпицентр» (родная сборка, запуск из меню клиента): запросы к api/client по игровому
#    токену из командной строки: -- --epic-token=… --epic-login=… [--epic-api=https://…/api/client/]
# Ответы в обоих режимах одинаковые, остальная игра режима не знает.

signal round_started(result: Dictionary)
signal score_saved(result: Dictionary)
signal top_loaded(result: Dictionary)
signal purchase_done(result: Dictionary)

const DEFAULT_API: = "https://epicentre-cs.ru/api/client/"

var _bridge: JavaScriptObject

var _pending: = {}
var _next_id: = 0

# режим клиента
var _token: = ""
var _login: = ""
var _api: = DEFAULT_API
var _round: = ""


func _ready() -> void :
	# Конец забега ставит игру на паузу, а запросы (HTTPRequest — дети этого узла) должны дойти
	process_mode = Node.PROCESS_MODE_ALWAYS
	if OS.has_feature("web"):
		_bridge = JavaScriptBridge.get_interface("XmasBridge")
		return
	for arg in OS.get_cmdline_user_args():
		if arg.begins_with("--epic-token="):
			_token = arg.trim_prefix("--epic-token=")
		elif arg.begins_with("--epic-login="):
			_login = arg.trim_prefix("--epic-login=").uri_decode()
		elif arg.begins_with("--epic-api="):
			_api = arg.trim_prefix("--epic-api=")


func _client() -> bool:
	return _token != ""


func is_available() -> bool:
	return _bridge != null or _client()


func is_logged_in() -> bool:
	if _client():
		return true
	return is_available() and bool(_bridge.logged)


func user_login() -> String:
	if _client():
		return _login
	return str(_bridge.login) if is_logged_in() else ""




func start_round(confirm_no_xp: = false) -> void :
	if _client():
		_api_call({"action": "game_start", "confirm_no_xp": 1 if confirm_no_xp else 0}, func(data: Dictionary) -> void :
			_round = str(data.get("round", "")) if data.get("ok", false) else ""
			data.erase("round")
			round_started.emit(data))
		return
	if not is_available():
		round_started.emit({"ok": false, "mode": "offline"})
		return
	_bridge.start(confirm_no_xp, _callback( func(data: Dictionary) -> void :
		round_started.emit(data)))



func buy(kind: String, count: int) -> void :
	if _client():
		_api_call({"action": "bp_buy", "kind": kind, "count": count}, func(data: Dictionary) -> void :
			purchase_done.emit(data))
		return
	if not is_available():
		purchase_done.emit({"ok": false, "message": "Покупки доступны на сайте"})
		return
	_bridge.buy(kind, count, _callback( func(data: Dictionary) -> void :
		purchase_done.emit(data)))



func submit_run(run: Dictionary) -> void :
	if _client():
		var form: = {"action": "game_submit", "round": _round}
		for key in ["score", "distance", "gifts", "crates", "enemies", "duration"]:
			form[key] = int(run.get(key, 0))
		_round = ""
		_api_call(form, func(data: Dictionary) -> void :
			score_saved.emit(data))
		return
	if not is_available():
		score_saved.emit({"ok": false, "message": "Таблица лидеров доступна на сайте"})
		return
	var payload: = JSON.stringify(run)
	_bridge.submit(payload, _callback( func(data: Dictionary) -> void :
		score_saved.emit(data)))


func fetch_top() -> void :
	if _client():
		_api_call({"action": "top"}, func(data: Dictionary) -> void :
			top_loaded.emit(data))
		return
	if not is_available():
		top_loaded.emit({"ok": false, "entries": []})
		return
	_bridge.top(_callback( func(data: Dictionary) -> void :
		top_loaded.emit(data)))


func _callback(handler: Callable) -> JavaScriptObject:
	var id: = _next_id
	_next_id += 1
	var js_callback: = JavaScriptBridge.create_callback( func(args: Array) -> void :
		_pending.erase(id)
		var data: Variant = JSON.parse_string(str(args[0])) if args.size() > 0 else null
		handler.call(data if data is Dictionary else {"ok": false, "message": "Ошибка связи с сайтом"}))
	_pending[id] = js_callback
	return js_callback


# POST в api/client; в handler всегда приходит словарь {ok, ...}
func _api_call(form: Dictionary, handler: Callable) -> void :
	form["token"] = _token
	var request: = HTTPRequest.new()
	request.timeout = 20.0
	add_child(request)
	request.request_completed.connect( func(result: int, code: int, _headers: PackedStringArray, body: PackedByteArray) -> void :
		request.queue_free()
		var data: Variant = null
		if result == HTTPRequest.RESULT_SUCCESS:
			data = JSON.parse_string(body.get_string_from_utf8())
		if not data is Dictionary:
			data = {"ok": false, "message": "Сайт недоступен, попробуйте позже" if result != HTTPRequest.RESULT_SUCCESS else "Ошибка сайта (HTTP %d)" % code}
		elif str(data.get("error", "")) == "auth":
			data["message"] = "Сессия истекла — запустите игру из клиента заново"
		handler.call(data))
	var body: = HTTPClient.new().query_string_from_dict(form)
	var error: = request.request(_api, ["Content-Type: application/x-www-form-urlencoded"], HTTPClient.METHOD_POST, body)
	if error != OK:
		request.queue_free()
		handler.call({"ok": false, "message": "Не удалось отправить запрос"})
