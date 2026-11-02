# Epicentre Client

Клиент CS 1.6 сервера **Эпицентр Арена Страйка** на основе [NextClient](https://github.com/CS-NextClient/NextClient).

## Что отличается от NextClient

- **Главное меню Эпицентра** (`assets/platform/epicentre/menu`) — веб-страница во весь экран:
  карточка сервера 45.95.31.157:27115 с онлайном, картой и игроками, кнопка «Подключиться»,
  вкладка «Серверы» (общий мастер-лист, избранное, история) и сайт epicentre-cs.ru.
- **Chromium 150 вместо CEF1** (`nextclient/chromehtml`): свой `chromehtml.dll`, реализующий
  `ChromeHTML_Controller_001`. MOTD, меню и все HTML-окна — современный браузер. CEF лежит в
  `browser\cef\`, логи браузера — `browser\browser.log`, настройки — `browser\browser.ini`
  (`zoom`, `fit_width`, `frame_rate`, `debug_port`).
- **`window.nextclient`** работает в новом браузере: встроенные методы (`getCvarValue`,
  `connectedHost`, `gameUi.shown`, события подключения, `exec` для доверенных страниц,
  `matchmaking.getServerInfo/getPlayersInfo` прямым запросом A2S) и методы GameUI через
  интерфейс `NCL_BrowserApi_001` (списки серверов, избранное, правила, `optionsDialog.open`,
  события консоли). Новый метод: `matchmaking.getInternetServers()`.
- Доверенные страницы: встроенное меню `ncl://menu/` и домены из `platform/accepted_domains.txt`
  (`epicentre-cs.ru`).
- **Стандартное меню игры скрыто**: «Новая игра», «Настройки», «Игроки», «Отключиться», «Выход»
  стоят в верхней навигации страницы меню (`nextclient.gameUi.command(name)`). vgui-браузер
  серверов при старте не открывается.
- **Брендинг**: заголовок окна «Эпицентр Арена Страйка», иконка из логотипа сервера.
- **Без Discord**: статус в Discord отключён (нет `cl_discordrpc*`, нет схемы `discord-…://`).
  Ссылка на проект — [группа ВКонтакте](https://vk.ru/arenatira); она открывается в системном
  браузере через `nextclient.openExternal(url)` (только https-ссылки vk.ru, vk.com и epicentre-cs.ru).
- **Нужен запущенный Steam**: режим без Steam есть только в закрытой официальной сборке NextClient.

## Меню

Меню — обычные HTML/CSS/JS в `assets/platform/epicentre/menu`, в игре открываются по адресу
`ncl://menu/index.html`. Если положить в `platform/mmb_home_page.html` другой адрес, откроется он.
Вне игры страницу можно открыть в браузере — она покажет примерные данные.

## Сборка (Windows)

Требования NextClient (MSVC, CMake, Git LFS) плюс Ninja. Из обычной командной строки:

```bat
set EPICENTRE_CEF_ROOT=<распакованный cef_binary_150.0.23+g4ccc4ca+chromium-150.0.7871.287_windows32_minimal>
tools\build-windows.cmd Release "C:\Games\CS 1.6 - Epicentre"
```

Без `EPICENTRE_CEF_ROOT` CEF скачается при конфигурации. Загрузки vcpkg идут через
`tools/vcpkg-fetch.ps1` (работает за HTTP-прокси, где встроенный загрузчик vcpkg не справляется).

Только модуль браузера с тестами (без vcpkg):

```bat
tools\build-chromehtml-tests.cmd <путь к CEF>
```

Дизайн и решения: [2026-11-02-design.md](2026-11-02-design.md).
