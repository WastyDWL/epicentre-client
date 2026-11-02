# Новогодняя игра (батлпасс «Эпицентр»)

Проект Godot 4.7.2 (GL Compatibility). Исходник восстановлен из веб-сборки сайта
(`modules_extra/xmas_game/game/index.pck`) инструментом GDRE Tools 2.7.4: скрипты декомпилированы,
комментарии оригинала потеряны.

## Две сборки из одного проекта

- **Web** — для страницы `/xmas_game` на сайте: `build/web/*` → `modules_extra/xmas_game/game/`
  (и поднять `?v=` у `GAME` в `templates/slim/js/game.js`).
- **Windows Desktop** — родная сборка для клиента: `build/windows/XmasGame.exe` →
  `assets/epicentre_games/xmas/XmasGame.exe` (Git LFS). Клиент запускает её кнопкой «Играть»
  во вкладке «Батлпасс».

```
godot --headless --path games/xmas_game --export-release "Windows Desktop" build/windows/XmasGame.exe
godot --headless --path games/xmas_game --export-release "Web" build/web/index.html
```

## Связь с сайтом — `scripts/site_bridge.gd`

- в браузере — `window.XmasBridge` страницы сайта (сессия сайта);
- в родной сборке — запросы к `https://epicentre-cs.ru/api/client/` по игровому токену, который
  клиент передаёт при запуске: `XmasGame.exe -- --epic-token=… --epic-login=…`
  (токен на 12 часов, только `game_start`, `game_submit`, `top` и покупка билетов).

Без токена родная сборка запускается как демо: результаты не сохраняются.
