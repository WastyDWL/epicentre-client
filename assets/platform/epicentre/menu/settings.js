// Epicentre options screen (replaces the vgui "Options" dialog). Cvars are read with
// nextclient.getCvarValue and written with nextclient.exec as soon as a control changes;
// video modes and key bindings go through nextclient.settings (GameUI).
(() => {
  const nc = window.nextclient;
  const api = nc && nc.settings;
  const preview = !nc;
  const $ = (id) => document.getElementById(id);
  const esc = (s) => String(s).replace(/[&<>"]/g, (c) => ({ '&': '&amp;', '<': '&lt;', '>': '&gt;', '"': '&quot;' })[c]);

  const OFFON = [['0', 'Выкл'], ['1', 'Вкл']];

  // label, cvar, default; rows whose cvar the game does not have are hidden
  const CATS = [
    { id: 'game', title: 'Игра', tabs: [
      { id: 'crosshair', title: 'Прицел', scene: { move: 3, demo: 0 },
        presets: [
          ['Классический', { cl_crosshair_type: 'def', cl_crosshair_size: 'def', cl_crosshair_color: 'def', cl_crosshair_translucent: 'def', cl_dynamiccrosshair: 'def' }],
          ['Точка', { cl_crosshair_type: '3', cl_dynamiccrosshair: '0' }],
          ['Статичный', { cl_dynamiccrosshair: '0' }],
        ],
        groups: [
        { title: 'Прицел', crosshairPreview: true, rows: [
          { type: 'select', cvar: 'cl_crosshair_type', label: 'Форма', def: '0', options: [['0', 'Крест'], ['1', 'Т-образный'], ['2', 'Круг'], ['3', 'Точка']] },
          { type: 'select', cvar: 'cl_crosshair_size', label: 'Размер', def: 'auto', options: [['auto', 'Авто'], ['extra_small', 'Очень маленький'], ['small', 'Маленький'], ['medium', 'Средний'], ['large', 'Большой']] },
          { type: 'color', cvar: 'cl_crosshair_color', label: 'Цвет', def: '50 250 50' },
          { type: 'select', cvar: 'cl_crosshair_translucent', label: 'Полупрозрачный', def: '1', options: OFFON },
          { type: 'select', cvar: 'cl_dynamiccrosshair', label: 'Расходится при движении и стрельбе', def: '1', options: OFFON },
        ] },
      ] },
      { id: 'weapon', title: 'Оружие', scene: { move: 3, demo: 0 },
        presets: [
          ['По умолчанию', { viewmodel_offset_x: 'def', viewmodel_offset_y: 'def', viewmodel_offset_z: 'def', viewmodel_fov: 'def' }],
          ['По центру', { viewmodel_offset_x: '-1.5', viewmodel_offset_y: '1', viewmodel_offset_z: '0.5' }],
          ['Широкий обзор', { viewmodel_fov: '100' }],
        ],
        groups: [
        { title: 'Модель оружия', rows: [
          { type: 'slider', cvar: 'viewmodel_fov', label: 'Поле зрения модели', def: '90', min: 70, max: 100, step: 1 },
          { type: 'slider', cvar: 'viewmodel_offset_x', label: 'Сдвиг: влево / вправо', def: '0', min: -8, max: 8, step: 0.1 },
          { type: 'slider', cvar: 'viewmodel_offset_y', label: 'Сдвиг: назад / вперёд', def: '0', min: -8, max: 8, step: 0.1 },
          { type: 'slider', cvar: 'viewmodel_offset_z', label: 'Сдвиг: вверх / вниз', def: '0', min: -8, max: 8, step: 0.1 },
          { type: 'select', cvar: 'viewmodel_disable_shift', label: 'Не сдвигать модель при прицеливании', def: '0', options: OFFON },
          { type: 'select', cvar: 'cl_righthand', label: 'Оружие в руке', def: '1', options: [['1', 'Правой'], ['0', 'Левой']] },
        ] },
        { title: 'Переключение оружия', rows: [
          { type: 'select', cvar: 'hud_fastswitch', label: 'Быстрое переключение', def: '0', options: OFFON },
          { type: 'select', cvar: '_cl_autowepswitch', label: 'Брать подобранное оружие в руки', def: '1', options: OFFON },
        ] },
      ] },
      { id: 'bob', title: 'Покачивание', scene: { move: 0, demo: 0 },
        presets: [
          ['Классический', { cl_bobstyle: '0', cl_bob: 'def', cl_bobcycle: 'def', cl_bobup: 'def' }],
          ['Современный', { cl_bobstyle: '2', cl_bobcycle: 'def', cl_bobup: 'def', cl_bobamt_vert: 'def', cl_bobamt_lat: 'def', cl_bob_lower_amt: 'def' }],
          ['Без покачивания', { cl_bob: '0', cl_bobamt_vert: '0', cl_bobamt_lat: '0', cl_bob_lower_amt: '0' }],
        ],
        groups: [
        { title: 'Покачивание оружия', rows: [
          { type: 'select', cvar: 'cl_bobstyle', label: 'Стиль', def: '0', options: [['0', 'Классический'], ['1', 'Классический с раскачкой'], ['2', 'Современный']] },
          { type: 'slider', cvar: 'cl_bob', label: 'Сила покачивания', def: '0.01', min: 0, max: 0.05, step: 0.001 },
          { type: 'slider', cvar: 'cl_bobcycle', label: 'Частота шагов', def: '0.8', min: 0.1, max: 2, step: 0.01 },
          { type: 'slider', cvar: 'cl_bobup', label: 'Подъём на шаге', def: '0.5', min: 0.05, max: 0.95, step: 0.01 },
          { type: 'slider', cvar: 'cl_bobamt_vert', label: 'Амплитуда по вертикали', def: '0.13', min: 0, max: 0.4, step: 0.01 },
          { type: 'slider', cvar: 'cl_bobamt_lat', label: 'Амплитуда в стороны', def: '0.32', min: 0, max: 0.8, step: 0.01 },
          { type: 'slider', cvar: 'cl_bob_lower_amt', label: 'Опускание при беге', def: '8', min: 0, max: 30, step: 1 },
          { type: 'select', cvar: 'cl_bob_camera', label: 'Покачивание камеры', def: '1', options: OFFON },
        ] },
      ] },
      { id: 'inertia', title: 'Инерция', scene: { move: 3, demo: 1 },
        presets: [
          ['Выкл', { viewmodel_lag_style: '0' }],
          ['Half-Life 2', { viewmodel_lag_style: '1', viewmodel_lag_scale: 'def', viewmodel_lag_speed: 'def' }],
          ['CS: Source', { viewmodel_lag_style: '2', viewmodel_lag_scale: 'def' }],
        ],
        groups: [
        { title: 'Инерция модели', rows: [
          { type: 'select', cvar: 'viewmodel_lag_style', label: 'Стиль', def: '0', options: [['0', 'Выкл'], ['1', 'Как в Half-Life 2'], ['2', 'Как в CS: Source']] },
          { type: 'slider', cvar: 'viewmodel_lag_scale', label: 'Сила', def: '1.0', min: 0, max: 5, step: 0.01 },
          { type: 'slider', cvar: 'viewmodel_lag_speed', label: 'Скорость возврата', def: '8.0', min: 1, max: 20, step: 0.1 },
        ] },
      ] },
      { id: 'camera', title: 'Камера', scene: { move: 2, demo: 2 },
        presets: [
          ['Спокойная', { cl_rollangle: 'def', camera_movement_scale: '0', camera_movement_interp: 'def' }],
          ['Как в Quake', { cl_rollangle: '2', cl_rollspeed: 'def' }],
          ['Кинематографичная', { camera_movement_scale: '1.5', camera_movement_interp: '0.1' }],
        ],
        groups: [
        { title: 'Камера', rows: [
          { type: 'slider', cvar: 'cl_rollangle', label: 'Наклон при стрейфе', def: '0', min: 0, max: 10, step: 0.1 },
          { type: 'slider', cvar: 'cl_rollspeed', label: 'Скорость наклона', def: '200', min: 10, max: 400, step: 1 },
          { type: 'slider', cvar: 'camera_movement_scale', label: 'Движение камеры за анимацией', def: '1', min: 0, max: 2, step: 0.01 },
          { type: 'slider', cvar: 'camera_movement_interp', label: 'Плавность движения камеры', def: '0', min: 0, max: 0.5, step: 0.01 },
        ] },
      ] },
      { id: 'hud', title: 'Интерфейс', groups: [
        // client_mini's CS2 look: each part can go back to the stock one
        { title: 'Стиль интерфейса', rows: [
          { type: 'select', cvar: 'cl_hud_cs2', label: 'HUD (здоровье, патроны, деньги, счёт, таблица Tab)', def: '1', options: [['1', 'Новый (CS2)'], ['0', 'Классический']] },
          { type: 'select', cvar: 'cl_hud_cs2_cards', label: 'Карточки убийств', def: '1', options: [['1', 'Как в CS2'], ['2', 'Классические тузы'], ['0', 'Без карточек']] },
          { type: 'select', cvar: 'cl_buy_cs2', label: 'Меню закупки', def: '1', options: [['1', 'Новое (CS2)'], ['0', 'Классическое']] },
        ] },
        { title: 'Профиль', rows: [
          { type: 'text', cvar: 'name', label: 'Имя игрока', max: 31 },
        ] },
        { title: 'HUD', rows: [
          { type: 'select', cvar: 'hud_centerid', label: 'Имя игрока под прицелом по центру', def: '0', options: OFFON },
          { type: 'select', cvar: 'cl_radartype', label: 'Радар', def: '0', options: [['0', 'Классический'], ['1', 'Прозрачный']] },
          { type: 'select', cvar: 'cl_showfps', label: 'Показывать FPS', def: '0', options: OFFON },
          { type: 'select', cvar: 'net_graph', label: 'Сетевой график', def: '0', options: [['0', 'Выкл'], ['1', '1'], ['2', '2'], ['3', '3']] },
        ] },
      ] },
      { id: 'net', title: 'Сеть', groups: [
        { title: 'Сеть', rows: [
          { type: 'select', cvar: 'rate', label: 'Скорость соединения (rate)', def: '25000', options: [['25000', '25 000'], ['50000', '50 000'], ['100000', '100 000']] },
          { type: 'slider', cvar: 'cl_updaterate', label: 'Обновлений с сервера в секунду', def: '30', min: 20, max: 102, step: 1 },
          { type: 'slider', cvar: 'cl_cmdrate', label: 'Команд на сервер в секунду', def: '30', min: 20, max: 105, step: 1 },
          { type: 'slider', cvar: 'ex_interp', label: 'Интерполяция (ex_interp)', def: '0.1', min: 0, max: 0.1, step: 0.001 },
        ] },
      ] },
    ] },
    { id: 'video', title: 'Видео', tabs: [
      { id: 'display', title: 'Экран', groups: [
        { title: 'Режим экрана', video: true, rows: [] },
        { title: 'Изображение', rows: [
          { type: 'slider', cvar: 'brightness', label: 'Яркость', def: '1', min: 0, max: 2, step: 0.01 },
          { type: 'slider', cvar: 'gamma', label: 'Гамма', def: '2.5', min: 1, max: 3, step: 0.01 },
          { type: 'select', cvar: 'gl_vsync', label: 'Вертикальная синхронизация', def: '0', options: OFFON },
          { type: 'slider', cvar: 'fps_max', label: 'Ограничение FPS', def: '100', min: 30, max: 1000, step: 1 },
        ] },
      ] },
    ] },
    { id: 'audio', title: 'Аудио', tabs: [
      { id: 'sound', title: 'Звук', groups: [
        { title: 'Громкость', rows: [
          { type: 'slider', cvar: 'volume', label: 'Громкость игры', def: '0.8', min: 0, max: 1, step: 0.01, pct: true },
          { type: 'slider', cvar: 'MP3Volume', label: 'Громкость музыки', def: '0.8', min: 0, max: 1, step: 0.01, pct: true },
          { type: 'select', cvar: 'hisound', label: 'Качество звука', def: '1', options: [['1', 'Высокое'], ['0', 'Низкое']] },
        ] },
        { title: 'Голосовой чат', rows: [
          { type: 'select', cvar: 'voice_enable', label: 'Голосовой чат', def: '1', options: OFFON },
          { type: 'slider', cvar: 'voice_scale', label: 'Громкость других игроков', def: '1', min: 0, max: 1, step: 0.01, pct: true },
        ] },
      ] },
    ] },
    { id: 'input', title: 'Клавиатура / мышь', tabs: [
      { id: 'mouse', title: 'Мышь', groups: [
        { title: 'Мышь', rows: [
          { type: 'slider', cvar: 'sensitivity', label: 'Чувствительность', def: '3', min: 0.2, max: 20, step: 0.01 },
          { type: 'slider', cvar: 'zoom_sensitivity_ratio', label: 'Чувствительность в прицеле', def: '1.2', min: 0.1, max: 3, step: 0.01 },
          { type: 'select', cvar: 'm_rawinput', label: 'Прямой ввод (raw input)', def: '1', options: OFFON },
          { type: 'select', cvar: 'm_filter', label: 'Сглаживание мыши', def: '0', options: OFFON },
          { type: 'invert', cvar: 'm_pitch', label: 'Инвертировать по вертикали', def: '0.022' },
        ] },
      ] },
      { id: 'keys', title: 'Клавиши', keys: true, groups: [] },
    ] },
  ];

  // ---- cvar access ----
  const sampleCvars = { name: 'Игрок', cl_crosshair_color: '50 250 50' };
  function getCvar(row) {
    if (preview) return row.cvar in sampleCvars ? sampleCvars[row.cvar] : row.def ?? '';
    return nc.getCvarValue(row.cvar);
  }
  function setCvar(name, value) {
    const v = String(value).replace(/["\\;\r\n]/g, '');
    if (preview) { sampleCvars[name] = v; return; }
    nc.exec(`${name} "${v}"`);
  }
  const num = (v) => (Number.isFinite(+v) ? +v : 0);
  function fmt(row, v) {
    if (row.pct) return Math.round(num(v) * 100) + '%';
    const d = row.step >= 1 ? 0 : String(row.step).split('.')[1].length;
    return num(v).toFixed(d);
  }

  // ---- state ----
  let cat = CATS[0];
  let tab = cat.tabs[0];
  let query = '';
  let video = null;      // {modes, width, height, windowed, lowDetail} from GameUI
  let videoDraft = null;
  let binds = null;      // {sections, keys:{key:binding}, defaults}
  let capture = null;    // {binding, slot, button}

  // ---- rendering ----
  function render() {
    const root = $('page-settings');
    if (!root) return;
    root.innerHTML = `
      <div class="st-head">
        <label class="st-search"><span>⌕</span><input id="st-search" type="search" placeholder="Поиск настройки" value="${esc(query)}"></label>
        <nav class="st-cats">${CATS.map((c) => `<button class="st-cat${c === cat && !query ? ' on' : ''}" data-st-cat="${c.id}">${c.title}</button>`).join('')}</nav>
      </div>
      ${query ? '' : `<nav class="st-tabs">${cat.tabs.map((t) => `<button class="st-tab${t === tab ? ' on' : ''}" data-st-tab="${t.id}">${t.title}</button>`).join('')}</nav>`}
      ${query || !tab.scene ? '' : renderStage()}
      <div class="st-body" id="st-body">${query ? renderSearch() : renderTab(tab)}</div>
      <div class="st-foot">
        <button class="st-legacy" data-st-legacy>Дополнительно: классическое окно</button>
        ${query ? '' : `<button class="st-reset" data-st-reset>⟲ Сбросить «${esc(tab.title)}»</button>`}
      </div>`;
    const input = $('st-search');
    input.addEventListener('input', () => {
      query = input.value.trim().toLowerCase();
      const pos = input.selectionStart;
      render();
      const again = $('st-search');
      again.focus();
      again.setSelectionRange(pos, pos);
    });
    drawCrosshair();
    updateScene();
  }

  // ---- weapon preview (GameUI draws it over #st-scene) ----
  const MOVES = [[0, 'Бег'], [1, 'Ходьба'], [2, 'Стрейф'], [3, 'Стоя']];
  let sceneMove = 3;
  let sceneKey = '';

  function renderStage() {
    return `<div class="st-stage">
      <div class="st-moves">${MOVES.map(([m, t]) => `<button class="st-move${m === sceneMove ? ' on' : ''}" data-st-move="${m}">${t}</button>`).join('')}</div>
      <div class="st-scene" id="st-scene">${preview || !api ? '<canvas id="st-xhair" width="384" height="216"></canvas>' : ''}</div>
      <div class="st-presets"><span>Пресеты</span>${(tab.presets || []).map(([t], i) => `<button class="st-preset" data-st-preset="${i}">${esc(t)}</button>`).join('')}</div>
    </div>`;
  }

  function updateScene(force) {
    if (!api || !api.setGamePreview) return;
    const page = $('page-settings');
    const el = $('st-scene');
    const shown = el && page && page.classList.contains('active');
    const r = shown ? el.getBoundingClientRect() : null;
    const key = r ? [r.left, r.top, r.width, r.height, sceneMove, tab.scene.demo].map(Math.round).join() : 'off';
    if (key === sceneKey && !force) return;
    sceneKey = key;
    api.setGamePreview(r, sceneMove, r ? tab.scene.demo : 0).catch(() => {});
  }

  function applyPreset(i) {
    const [title, values] = tab.presets[i];
    for (const [cvar, v] of Object.entries(values)) {
      const row = rowOf(cvar);
      setCvar(cvar, v === 'def' && row ? row.def : v);
    }
    toast(`Пресет «${title}»`);
    render();
  }

  window.addEventListener('resize', () => updateScene(true));
  window.addEventListener('pagehide', () => api && api.setGamePreview && api.setGamePreview(null));

  function renderTab(t) {
    if (t.keys) return renderKeys();
    return t.groups.map((g) => renderGroup(g)).join('');
  }

  function renderGroup(g, rowsOverride) {
    const rows = (rowsOverride || g.rows).map(renderRow).filter(Boolean).join('');
    const extra = g.video ? renderVideo() : '';
    if (!rows && !extra) return '';
    return `<section class="st-group"><h3>${esc(g.title)}</h3>${extra}${rows}</section>`;
  }

  function renderSearch() {
    const out = [];
    for (const c of CATS) for (const t of c.tabs) for (const g of t.groups) {
      const rows = g.rows.filter((r) => r.label.toLowerCase().includes(query));
      if (rows.length) out.push(renderGroup({ ...g, video: false, crosshairPreview: false, title: `${c.title} › ${t.title} › ${g.title}` }, rows));
    }
    if ('клавиши'.includes(query) || 'бинд'.includes(query)) out.push('<p class="st-hint">Назначение клавиш — «Клавиатура / мышь» › «Клавиши».</p>');
    return out.join('') || '<p class="st-hint">Ничего не найдено</p>';
  }

  function renderRow(row) {
    const raw = getCvar(row);
    if (raw === null || raw === undefined) return '';  // the game has no such cvar
    const id = `st-${row.cvar}`;
    let control = '';
    if (row.type === 'select') {
      const cur = row.options.find(([v]) => String(v) === String(raw)) ||
        row.options.find(([v]) => num(v) === num(raw) && raw !== '');
      control = dropdown(row.cvar, row.options, cur ? cur[0] : raw, cur ? cur[1] : String(raw));
    } else if (row.type === 'invert') {
      const on = num(raw) < 0;
      control = dropdown(row.cvar, [['0', 'Выкл'], ['1', 'Вкл']], on ? '1' : '0', on ? 'Вкл' : 'Выкл', 'invert');
    } else if (row.type === 'slider') {
      control = `<input class="st-range" type="range" data-st-slider="${row.cvar}" min="${row.min}" max="${row.max}" step="${row.step}" value="${num(raw)}">
        <input class="st-num" data-st-num="${row.cvar}" value="${fmt(row, raw)}" spellcheck="false">`;
    } else if (row.type === 'text') {
      control = `<input class="st-text" data-st-text="${row.cvar}" maxlength="${row.max}" value="${esc(raw)}" spellcheck="false">`;
    } else if (row.type === 'color') {
      const [r, g, b] = String(raw).split(/\s+/).map(num);
      const hex = '#' + [r, g, b].map((x) => Math.max(0, Math.min(255, x | 0)).toString(16).padStart(2, '0')).join('');
      const swatches = [['50 250 50', 'Зелёный'], ['250 50 50', 'Красный'], ['50 50 250', 'Синий'], ['250 250 50', 'Жёлтый'], ['50 250 250', 'Голубой'], ['250 50 250', 'Розовый'], ['255 255 255', 'Белый']];
      control = `<div class="st-swatches">${swatches.map(([v, t]) => `<button class="st-sw${v === String(raw).trim() ? ' on' : ''}" title="${t}" data-st-color="${v}" style="background:rgb(${v.replace(/ /g, ',')})"></button>`).join('')}
        <label class="st-sw st-custom" title="Свой цвет" style="background:${hex}"><input type="color" data-st-picker value="${hex}"></label></div>`;
    }
    return `<div class="st-row" id="${id}"><span class="st-label">${esc(row.label)}</span><span class="st-ctl">${control}</span></div>`;
  }

  function dropdown(key, options, value, text, kind = 'cvar') {
    return `<div class="st-dd" data-st-dd="${esc(key)}" data-kind="${kind}">
      <button class="st-dd-btn">${esc(text)}<i>⌄</i></button>
      <div class="st-dd-menu">${options.map(([v, t]) => `<button class="${String(v) === String(value) ? 'on' : ''}" data-st-opt="${esc(v)}">${esc(t)}</button>`).join('')}</div>
    </div>`;
  }

  // ---- video ----
  function renderVideo() {
    if (!video) return '<div class="st-row"><span class="st-label">Загрузка…</span></div>';
    const d = videoDraft;
    const modes = video.modes.map(([w, h]) => [`${w}x${h}`, `${w} × ${h}`]);
    const changed = d.width !== video.width || d.height !== video.height || d.windowed !== video.windowed || d.lowDetail !== video.lowDetail;
    return `
      <div class="st-row"><span class="st-label">Разрешение</span><span class="st-ctl">${dropdown('res', modes, `${d.width}x${d.height}`, `${d.width} × ${d.height}`, 'video')}</span></div>
      <div class="st-row"><span class="st-label">Режим отображения</span><span class="st-ctl">${dropdown('windowed', [['0', 'Полный экран'], ['1', 'В окне']], d.windowed ? '1' : '0', d.windowed ? 'В окне' : 'Полный экран', 'video')}</span></div>
      <div class="st-row"><span class="st-label">Низкая детализация</span><span class="st-ctl">${dropdown('lowDetail', OFFON, d.lowDetail ? '1' : '0', d.lowDetail ? 'Вкл' : 'Выкл', 'video')}</span></div>
      ${changed ? `<div class="st-apply"><span>Видео перезапустится, игра на пару секунд погаснет.</span><button data-st-video-cancel>Отмена</button><button class="primary" data-st-video-apply>Применить</button></div>` : ''}`;
  }

  async function loadVideo() {
    try {
      video = preview
        ? { modes: [[1280, 720], [1600, 900], [1920, 1080], [1024, 768]], width: 1280, height: 720, windowed: true, lowDetail: false }
        : await api.getVideoState();
    } catch {
      video = null;
      return;
    }
    videoDraft = { ...video };
    if (tab.id === 'display' && !query) render();
  }

  // ---- key bindings ----
  const KEY_LABELS = { MOUSE1: 'ЛКМ', MOUSE2: 'ПКМ', MOUSE3: 'Колёсико', MOUSE4: 'Мышь 4', MOUSE5: 'Мышь 5', MWHEELUP: 'Колесо ↑', MWHEELDOWN: 'Колесо ↓',
    SPACE: 'Пробел', CTRL: 'Ctrl', SHIFT: 'Shift', ALT: 'Alt', TAB: 'Tab', ENTER: 'Enter', UPARROW: '↑', DOWNARROW: '↓', LEFTARROW: '←', RIGHTARROW: '→',
    SEMICOLON: ';', INS: 'Insert', DEL: 'Delete', PGUP: 'PgUp', PGDN: 'PgDn', HOME: 'Home', END: 'End', PAUSE: 'Pause' };
  const keyLabel = (k) => KEY_LABELS[k] || (k.length === 1 ? k.toUpperCase() : k.replace(/^KP_/, 'Num '));

  function keysFor(binding) {
    if (!binds) return [];
    return Object.keys(binds.keys).filter((k) => binds.keys[k].toLowerCase() === binding.toLowerCase());
  }

  function renderKeys() {
    if (!binds) return '<p class="st-hint">Загрузка…</p>';
    const head = '<p class="st-hint">Нажмите на ячейку и затем клавишу или кнопку мыши. Backspace — очистить, Esc — отмена.</p>';
    return head + binds.sections.map((s) => `
      <section class="st-group"><h3>${esc(s.title)}</h3>
        ${s.items.map((it) => {
          const keys = keysFor(it.binding);
          const slot = (i) => {
            const active = capture && capture.binding === it.binding && capture.slot === i;
            const k = keys[i];
            return `<button class="st-key${active ? ' wait' : ''}${k ? '' : ' unset'}" data-st-key="${esc(it.binding)}" data-slot="${i}" data-cur="${esc(k || '')}">${active ? 'Нажмите…' : k ? esc(keyLabel(k)) : '—'}</button>`;
          };
          return `<div class="st-row"><span class="st-label">${esc(it.label)}</span><span class="st-ctl">${slot(0)}${slot(1)}</span></div>`;
        }).join('')}
      </section>`).join('');
  }

  async function loadBinds() {
    try {
      binds = preview
        ? { sections: [{ title: 'Передвижение', items: [{ binding: '+forward', label: 'Вперёд' }, { binding: '+back', label: 'Назад' }, { binding: '+jump', label: 'Прыжок' }] }],
            keys: { w: '+forward', s: '+back', SPACE: '+jump', MWHEELDOWN: '+jump' }, defaults: [['w', '+forward'], ['s', '+back'], ['SPACE', '+jump']] }
        : await api.getKeyBindings();
    } catch {
      binds = null;
    }
    if (tab.keys && !query) render();
  }

  const CODE_KEYS = {
    Space: 'SPACE', Enter: 'ENTER', Tab: 'TAB', ShiftLeft: 'SHIFT', ShiftRight: 'SHIFT', ControlLeft: 'CTRL', ControlRight: 'CTRL',
    AltLeft: 'ALT', AltRight: 'ALT', ArrowUp: 'UPARROW', ArrowDown: 'DOWNARROW', ArrowLeft: 'LEFTARROW', ArrowRight: 'RIGHTARROW',
    Insert: 'INS', Delete: 'DEL', Home: 'HOME', End: 'END', PageUp: 'PGUP', PageDown: 'PGDN', Pause: 'PAUSE',
    Numpad0: 'KP_INS', Numpad1: 'KP_END', Numpad2: 'KP_DOWNARROW', Numpad3: 'KP_PGDN', Numpad4: 'KP_LEFTARROW', Numpad5: 'KP_5',
    Numpad6: 'KP_RIGHTARROW', Numpad7: 'KP_HOME', Numpad8: 'KP_UPARROW', Numpad9: 'KP_PGUP', NumpadEnter: 'KP_ENTER',
    NumpadDecimal: 'KP_DEL', NumpadAdd: 'KP_PLUS', NumpadSubtract: 'KP_MINUS', NumpadDivide: 'KP_SLASH', NumpadMultiply: '*',
    Minus: '-', Equal: '=', BracketLeft: '[', BracketRight: ']', Semicolon: 'SEMICOLON', Quote: "'", Comma: ',', Period: '.', Slash: '/',
  };
  function engineKey(e) {
    if (/^Key[A-Z]$/.test(e.code)) return e.code.slice(3).toLowerCase();
    if (/^Digit\d$/.test(e.code)) return e.code.slice(5);
    if (/^F\d{1,2}$/.test(e.code)) return e.code;
    return CODE_KEYS[e.code] || null;
  }

  async function assign(key) {
    const c = capture;
    capture = null;
    if (!c) return;
    const keys = keysFor(c.binding);
    const old = keys[c.slot];
    if (key && key === old) return render();
    try {
      if (old && !preview) await api.bindKey(old, '');
      if (old) delete binds.keys[old];
      if (key) {
        if (!preview) await api.bindKey(key, c.binding);
        binds.keys[key] = c.binding;
      }
    } catch {
      toast('Эту клавишу назначить нельзя');
    }
    render();
  }

  function startCapture(btn) {
    capture = { binding: btn.dataset.stKey, slot: +btn.dataset.slot };
    render();
  }

  document.addEventListener('keydown', (e) => {
    if (!capture) return;
    e.preventDefault();
    e.stopPropagation();
    if (e.code === 'Escape') { capture = null; return render(); }
    if (e.code === 'Backspace') return assign(null);
    const k = engineKey(e);
    if (k) assign(k);
  }, true);
  document.addEventListener('mousedown', (e) => {
    if (!capture) return;
    e.preventDefault();
    e.stopPropagation();
    assign(['MOUSE1', 'MOUSE3', 'MOUSE2', 'MOUSE4', 'MOUSE5'][e.button] || null);
  }, true);
  document.addEventListener('wheel', (e) => {
    if (!capture) return;
    e.preventDefault();
    assign(e.deltaY < 0 ? 'MWHEELUP' : 'MWHEELDOWN');
  }, { capture: true, passive: false });

  // ---- crosshair preview ----
  function drawCrosshair() {
    const cv = $('st-xhair');
    if (!cv) return;
    const g = cv.getContext('2d');
    const W = cv.width, H = cv.height;
    const bg = g.createLinearGradient(0, 0, 0, H);
    bg.addColorStop(0, '#9c7b52');
    bg.addColorStop(0.55, '#c9a46c');
    bg.addColorStop(1, '#7a5c3a');
    g.fillStyle = bg;
    g.fillRect(0, 0, W, H);
    const get = (c, d) => { const v = getCvar({ cvar: c, def: d }); return v == null ? d : v; };
    const [r, gr, b] = String(get('cl_crosshair_color', '50 250 50')).split(/\s+/).map(num);
    g.globalAlpha = get('cl_crosshair_translucent', '1') === '1' ? 0.7 : 1;
    g.fillStyle = g.strokeStyle = `rgb(${r},${gr},${b})`;
    const type = get('cl_crosshair_type', '0');
    const size = { extra_small: 4, small: 5, auto: 6, medium: 7, large: 9 }[get('cl_crosshair_size', 'auto')] || 6;
    const cx = W / 2 | 0, cy = H / 2 | 0, gap = 4;
    if (type === '3') {
      g.fillRect(cx - 1, cy - 1, 3, 3);
    } else if (type === '2') {
      g.lineWidth = 1.5;
      g.beginPath();
      g.arc(cx + 0.5, cy + 0.5, size + gap, 0, Math.PI * 2);
      g.stroke();
      g.fillRect(cx, cy, 1, 1);
    } else {
      g.fillRect(cx - gap - size * 2, cy, size * 2, 1);
      g.fillRect(cx + gap + 1, cy, size * 2, 1);
      g.fillRect(cx, cy + gap + 1, 1, size * 2);
      if (type !== '1') g.fillRect(cx, cy - gap - size * 2, 1, size * 2);
    }
    g.globalAlpha = 1;
  }

  // ---- events ----
  function closeDropdowns(except) {
    document.querySelectorAll('#page-settings .st-dd.open').forEach((d) => d !== except && d.classList.remove('open'));
  }

  function rowOf(cvar) {
    for (const c of CATS) for (const t of c.tabs) for (const g of t.groups) for (const r of g.rows) if (r.cvar === cvar) return r;
    return null;
  }

  function onClick(e) {
    const root = $('page-settings');
    if (!root || !root.contains(e.target)) return closeDropdowns();
    const t = e.target.closest('button, [data-st-dd]');
    if (!t) return closeDropdowns();
    if (t.dataset.stCat) {
      cat = CATS.find((c) => c.id === t.dataset.stCat);
      tab = cat.tabs[0];
      if (tab.scene) sceneMove = tab.scene.move;
      query = '';
      capture = null;
      return render();
    }
    if (t.dataset.stMove !== undefined) { sceneMove = +t.dataset.stMove; return render(); }
    if (t.dataset.stPreset !== undefined) return applyPreset(+t.dataset.stPreset);
    if (t.dataset.stTab) {
      tab = cat.tabs.find((x) => x.id === t.dataset.stTab);
      if (tab.scene) sceneMove = tab.scene.move;
      capture = null;
      render();
      if (tab.keys) loadBinds();
      return;
    }
    if (t.classList.contains('st-dd-btn')) {
      const dd = t.parentElement;
      closeDropdowns(dd);
      dd.classList.toggle('open');
      return;
    }
    if (t.dataset.stOpt !== undefined) {
      const dd = t.closest('.st-dd');
      const key = dd.dataset.stDd, kind = dd.dataset.kind, v = t.dataset.stOpt;
      dd.classList.remove('open');
      if (kind === 'video') {
        if (key === 'res') [videoDraft.width, videoDraft.height] = v.split('x').map(Number);
        else videoDraft[key] = v === '1';
      } else if (kind === 'invert') {
        const cur = Math.abs(num(getCvar({ cvar: key, def: '0.022' }))) || 0.022;
        setCvar(key, v === '1' ? -cur : cur);
      } else {
        setCvar(key, v);
      }
      return render();
    }
    if (t.dataset.stColor) { setCvar('cl_crosshair_color', t.dataset.stColor); return render(); }
    if (t.dataset.stKey !== undefined) return startCapture(t);
    if (t.hasAttribute('data-st-video-cancel')) { videoDraft = { ...video }; return render(); }
    if (t.hasAttribute('data-st-video-apply')) {
      if (preview) { video = { ...videoDraft }; return render(); }
      api.applyVideo(videoDraft.width, videoDraft.height, videoDraft.windowed, videoDraft.lowDetail)
        .then(() => { video = { ...videoDraft }; render(); }, () => toast('Не удалось сменить режим'));
      return;
    }
    if (t.hasAttribute('data-st-reset')) return resetTab();
    if (t.hasAttribute('data-st-legacy')) {
      if (preview) return toast('gameUi.command(Options)');
      nc.gameUi.command('Options').catch(() => toast('Окно недоступно'));
    }
  }

  async function resetTab() {
    if (tab.keys) {
      if (!binds) return;
      const actions = new Set(binds.sections.flatMap((s) => s.items.map((i) => i.binding.toLowerCase())));
      for (const [k, b] of Object.entries(binds.keys)) {
        if (!actions.has(b.toLowerCase())) continue;
        if (!preview) await api.bindKey(k, '').catch(() => {});
        delete binds.keys[k];
      }
      for (const [k, b] of binds.defaults) {
        if (!actions.has(b.toLowerCase())) continue;
        if (!preview) await api.bindKey(k, b).catch(() => {});
        binds.keys[k] = b;
      }
    } else {
      for (const g of tab.groups) for (const r of g.rows) if (r.def !== undefined && getCvar(r) != null) setCvar(r.cvar, r.def);
    }
    toast(`«${tab.title}»: по умолчанию`);
    render();
  }

  function onInput(e) {
    const el = e.target;
    if (el.dataset.stSlider) {
      const row = rowOf(el.dataset.stSlider);
      setCvar(row.cvar, el.value);
      const box = el.parentElement.querySelector('.st-num');
      if (box) box.value = fmt(row, el.value);
    } else if (el.hasAttribute('data-st-picker')) {
      const h = el.value;
      setCvar('cl_crosshair_color', [1, 3, 5].map((i) => parseInt(h.substr(i, 2), 16)).join(' '));
      el.parentElement.style.background = h;
      drawCrosshair();
    }
  }

  function onChange(e) {
    const el = e.target;
    if (el.dataset.stSlider) return drawCrosshair();
    if (el.dataset.stNum) {
      const row = rowOf(el.dataset.stNum);
      let v = parseFloat(String(el.value).replace(',', '.').replace('%', ''));
      if (!Number.isFinite(v)) return render();
      if (row.pct) v /= 100;
      v = Math.max(row.min, Math.min(row.max, v));
      setCvar(row.cvar, v);
      return render();
    }
    if (el.dataset.stText) {
      const v = el.value.replace(/["\\;\r\n]/g, '').trim();
      if (v) {
        setCvar(el.dataset.stText, v);
        toast('Сохранено');
        const p = $('profile');
        if (p && el.dataset.stText === 'name') p.innerHTML = 'Вы играете как <b>' + esc(v) + '</b>';
      }
      return render();
    }
    if (el.hasAttribute('data-st-picker')) render();
  }

  function toast(text) {
    const t = $('toast');
    if (!t) return;
    t.textContent = text;
    t.classList.add('show');
    clearTimeout(toast.timer);
    toast.timer = setTimeout(() => t.classList.remove('show'), 2200);
  }

  document.addEventListener('click', onClick);
  document.addEventListener('input', onInput);
  document.addEventListener('change', onChange);
  document.addEventListener('keydown', (e) => {
    if (e.key === 'Enter' && e.target.matches && e.target.matches('.st-num, .st-text')) e.target.blur();
  });

  window.EpicSettings = {
    open() {
      render();
      requestAnimationFrame(() => updateScene(true));  // the page is laid out by now
      if (!video) loadVideo();
      if (tab.keys) loadBinds();
    },
    close() {
      capture = null;
      updateScene(true);  // the page is hidden: takes the preview away
    },
  };
})();
