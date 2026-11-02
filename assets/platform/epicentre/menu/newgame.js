// Epicentre "New game" page (replaces the vgui Create Server dialog): map grid on the left,
// server / rules / bots on the right, start button. The listen server is started by GameUI
// (nextclient.game.start), which applies rules and bot cvars once the map is up.
(() => {
  const nc = window.nextclient;
  const game = nc && nc.game;
  const preview = !nc;
  const $ = (id) => document.getElementById(id);
  const esc = (s) => String(s).replace(/[&<>"]/g, (c) => ({ '&': '&amp;', '<': '&lt;', '>': '&gt;', '"': '&quot;' })[c]);
  const STORE = 'epicentre.newgame';

  const MODES = {
    de: ['Обезвреживание', 'de'], cs: ['Спасение заложников', 'cs'], as: ['Убийство VIP', 'as'],
    es: ['Побег', 'other'], aim: ['Аим', 'other'], awp: ['AWP', 'other'], fy: ['Бой', 'other'], ka: ['Ножи', 'other'],
  };
  const FILTERS = [['all', 'Все карты'], ['de', 'Обезвреживание'], ['cs', 'Заложники'], ['as', 'VIP'], ['other', 'Другие']];
  const modeOf = (m) => MODES[m.split('_')[0]] || ['Другое', 'other'];

  const OFFON = [['0', 'Выкл'], ['1', 'Вкл']];
  // [key, label, kind, default, extra]
  const SECTIONS = [
    { title: 'Сервер', rows: [
      ['hostname', 'Название', 'text', 'Эпицентр — тренировка', { max: 63 }],
      ['maxplayers', 'Игроков', 'slider', '20', { min: 2, max: 32, step: 1 }],
      ['sv_password', 'Пароль', 'text', '', { max: 31, placeholder: 'без пароля' }],
    ] },
    { title: 'Правила', rows: [
      ['mp_roundtime', 'Время раунда, мин', 'slider', '2.5', { min: 1, max: 9, step: 0.5 }],
      ['mp_freezetime', 'Заморозка, сек', 'slider', '6', { min: 0, max: 20, step: 1 }],
      ['mp_buytime', 'Время закупки, мин', 'slider', '1.5', { min: 0.25, max: 5, step: 0.25 }],
      ['mp_startmoney', 'Стартовые деньги', 'slider', '800', { min: 800, max: 16000, step: 100 }],
      ['mp_timelimit', 'Лимит карты, мин (0 — нет)', 'slider', '0', { min: 0, max: 60, step: 5 }],
      ['mp_maxrounds', 'Макс. раундов (0 — нет)', 'slider', '0', { min: 0, max: 50, step: 1 }],
      ['mp_friendlyfire', 'Огонь по своим', 'select', '0', { options: OFFON }],
      ['mp_footsteps', 'Звук шагов', 'select', '1', { options: OFFON }],
      ['mp_flashlight', 'Фонарик', 'select', '1', { options: OFFON }],
    ] },
    { title: 'Боты', bots: true, rows: [
      ['bots', 'Добавить ботов', 'select', '1', { options: OFFON, local: true }],
      ['bot_quota', 'Количество ботов', 'slider', '9', { min: 1, max: 31, step: 1, bot: true }],
      ['bot_difficulty', 'Сложность', 'segment', '0', { options: [['0', 'Лёгкие'], ['1', 'Средние'], ['2', 'Сложные'], ['3', 'Эксперты']], bot: true }],
      ['bot_join_team', 'Команда ботов', 'select', 'any', { options: [['any', 'Любая'], ['T', 'Террористы'], ['CT', 'Спецназ']], bot: true }],
      ['bot_chatter', 'Радиопереговоры', 'select', 'normal', { options: [['off', 'Выкл'], ['radio', 'Только радио'], ['minimal', 'Минимум'], ['normal', 'Обычные']], bot: true }],
      ['bot_join_after_player', 'Ждать, пока зайдёт игрок', 'select', '1', { options: OFFON, bot: true }],
      ['weapons', 'Оружие ботов', 'weapons', '', { bot: true }],
    ] },
  ];
  const WEAPONS = [['bot_allow_pistols', 'Пистолеты'], ['bot_allow_shotguns', 'Дробовики'], ['bot_allow_sub_machine_guns', 'ПП'],
    ['bot_allow_rifles', 'Винтовки'], ['bot_allow_snipers', 'Снайперские'], ['bot_allow_machine_guns', 'Пулемёты'],
    ['bot_allow_grenades', 'Гранаты'], ['bot_allow_shield', 'Щит']];

  // ---- state ----
  let maps = null;
  let filter = 'all';
  let query = '';
  let selected = '*';
  let starting = false;
  const values = {};

  function load() {
    for (const s of SECTIONS) for (const [k, , kind, def] of s.rows) if (kind !== 'weapons') values[k] = def;
    for (const [k] of WEAPONS) values[k] = '1';
    try {
      const saved = JSON.parse(localStorage.getItem(STORE) || '{}');
      Object.assign(values, saved.values || {});
      if (saved.map) selected = saved.map;
    } catch { /* no storage on this origin: defaults */ }
  }
  function save() {
    try { localStorage.setItem(STORE, JSON.stringify({ values, map: selected })); } catch { /* ignore */ }
  }

  // ---- render ----
  function render() {
    const root = $('page-newgame');
    if (!root) return;
    const scroll = root.querySelector('.ng-maps')?.scrollTop || 0;
    root.innerHTML = `
      <div class="ng-head">
        <h2>Новая игра</h2>
        <nav class="ng-filters">${FILTERS.map(([f, t]) => `<button class="ng-filter${f === filter ? ' on' : ''}" data-ng-filter="${f}">${t}</button>`).join('')}</nav>
        <label class="st-search"><span>⌕</span><input id="ng-search" type="search" placeholder="Поиск карты" value="${esc(query)}"></label>
      </div>
      <div class="ng-main">
        <div class="ng-maps">${renderMaps()}</div>
        <aside class="ng-side">
          <div class="ng-opts">${SECTIONS.map(renderSection).join('')}</div>
          <div class="ng-go">
            <div class="ng-sel"><span>Карта</span><b>${selected === '*' ? 'Случайная' : esc(selected)}</b></div>
            <button class="ng-start" data-ng-start ${starting || !maps || !maps.length ? 'disabled' : ''}>${starting ? 'Запуск…' : 'Начать'}</button>
          </div>
        </aside>
      </div>`;
    root.querySelector('.ng-maps').scrollTop = scroll;
    const input = $('ng-search');
    input.addEventListener('input', () => {
      query = input.value.trim().toLowerCase();
      root.querySelector('.ng-maps').innerHTML = renderMaps();
    });
  }

  function renderMaps() {
    if (!maps) return '<p class="st-hint">Загрузка карт…</p>';
    if (!maps.length) return '<p class="st-hint">Карты не найдены</p>';
    const list = maps.filter((m) => (filter === 'all' || modeOf(m)[1] === filter) && (!query || m.includes(query)));
    const card = (m) => {
      const [mode, kind] = modeOf(m);
      const prefix = m.split('_')[0];
      const name = MODES[prefix] && m.length > prefix.length + 1 ? m.slice(prefix.length + 1) : m;
      return `<button class="ng-card k-${kind}${m === selected ? ' on' : ''}" data-ng-map="${esc(m)}">
        <span class="ng-mode">${esc(mode)}</span><span class="ng-name">${esc(name.replace(/_/g, ' '))}</span><span class="ng-file">${esc(m)}</span></button>`;
    };
    const random = `<button class="ng-card k-random${selected === '*' ? ' on' : ''}" data-ng-map="*">
      <span class="ng-mode">Любой режим</span><span class="ng-name">Случайная карта</span><span class="ng-file">${maps.length} карт</span></button>`;
    return `<div class="ng-grid">${filter === 'all' && !query ? random : ''}${list.map(card).join('') || '<p class="st-hint">Ничего не найдено</p>'}</div>`;
  }

  function renderSection(s) {
    const botsOff = values.bots === '0';
    return `<section class="ng-section"><h3>${s.title}</h3>${s.rows.map((r) => renderRow(r, s.bots && botsOff && r[4].bot)).join('')}</section>`;
  }

  function renderRow([key, label, kind, , x], disabled) {
    const v = values[key];
    let ctl = '';
    if (kind === 'text') {
      ctl = `<input class="st-text ng-text" data-ng-text="${key}" maxlength="${x.max}" placeholder="${esc(x.placeholder || '')}" value="${esc(v)}" spellcheck="false">`;
    } else if (kind === 'slider') {
      ctl = `<input class="st-range ng-range" type="range" data-ng-slider="${key}" min="${x.min}" max="${x.max}" step="${x.step}" value="${v}">
        <input class="st-num ng-num" data-ng-num="${key}" value="${v}" spellcheck="false">`;
    } else if (kind === 'select') {
      const cur = x.options.find(([o]) => o === String(v)) || x.options[0];
      ctl = `<div class="st-dd" data-ng-dd="${key}"><button class="st-dd-btn">${esc(cur[1])}<i>⌄</i></button>
        <div class="st-dd-menu">${x.options.map(([o, t]) => `<button class="${o === String(v) ? 'on' : ''}" data-ng-opt="${esc(o)}">${esc(t)}</button>`).join('')}</div></div>`;
    } else if (kind === 'segment') {
      ctl = `<div class="ng-seg">${x.options.map(([o, t]) => `<button class="${o === String(v) ? 'on' : ''}" data-ng-seg="${key}" data-v="${o}">${t}</button>`).join('')}</div>`;
    } else if (kind === 'weapons') {
      return `<div class="ng-row ng-wrow${disabled ? ' off' : ''}"><span class="st-label">${label}</span>
        <div class="ng-weapons">${WEAPONS.map(([k, t]) => `<button class="${values[k] === '1' ? 'on' : ''}" data-ng-weapon="${k}">${t}</button>`).join('')}</div></div>`;
    }
    return `<div class="ng-row${kind === 'segment' ? ' ng-col' : ''}${disabled ? ' off' : ''}"><span class="st-label">${label}</span><span class="st-ctl">${ctl}</span></div>`;
  }

  // ---- actions ----
  async function loadMaps() {
    try {
      maps = preview
        ? ['de_dust2', 'de_inferno', 'de_nuke', 'de_train', 'de_aztec', 'cs_office', 'cs_italy', 'cs_assault', 'as_oilrig', 'awp_india', 'fy_snow']
        : (await game.getMaps()).maps;
    } catch {
      maps = [];
    }
    if (selected !== '*' && !maps.includes(selected)) selected = '*';
    render();
  }

  async function start() {
    if (starting) return;
    const opts = { map: selected };
    for (const s of SECTIONS) for (const [k, , kind, , x] of s.rows) {
      if (kind === 'weapons' || x.local) continue;
      if (x.bot && values.bots === '0') continue;
      opts[k] = values[k];
    }
    if (values.bots === '0') opts.bot_quota = '0';
    else for (const [k] of WEAPONS) opts[k] = values[k];
    save();
    if (preview) return toast('start ' + JSON.stringify(opts));
    starting = true;
    render();
    try {
      const r = await game.start(opts);
      toast(`Загрузка карты ${r.map}…`);
    } catch {
      toast('Не удалось запустить игру');
    }
    starting = false;
    render();
  }

  function setValue(key, v) {
    values[key] = String(v);
    save();
  }

  function onClick(e) {
    const root = $('page-newgame');
    if (!root || !root.contains(e.target)) return;
    const t = e.target.closest('button');
    if (!t) return;
    if (t.dataset.ngFilter) { filter = t.dataset.ngFilter; return render(); }
    if (t.dataset.ngMap) { selected = t.dataset.ngMap; save(); return render(); }
    if (t.hasAttribute('data-ng-start')) return start();
    if (t.dataset.ngSeg) { setValue(t.dataset.ngSeg, t.dataset.v); return render(); }
    if (t.dataset.ngWeapon) { setValue(t.dataset.ngWeapon, values[t.dataset.ngWeapon] === '1' ? '0' : '1'); return render(); }
    if (t.classList.contains('st-dd-btn')) {
      const dd = t.parentElement;
      const open = !dd.classList.contains('open');
      root.querySelectorAll('.st-dd.open').forEach((d) => d.classList.remove('open'));
      dd.classList.toggle('open', open);
      return;
    }
    if (t.dataset.ngOpt !== undefined) {
      setValue(t.closest('[data-ng-dd]').dataset.ngDd, t.dataset.ngOpt);
      return render();
    }
  }

  function rowOf(key) {
    for (const s of SECTIONS) for (const r of s.rows) if (r[0] === key) return r;
    return null;
  }

  function onInput(e) {
    const el = e.target;
    if (el.dataset.ngSlider) {
      setValue(el.dataset.ngSlider, el.value);
      const box = el.parentElement.querySelector('.ng-num');
      if (box) box.value = el.value;
    }
  }

  function onChange(e) {
    const el = e.target;
    if (el.dataset.ngNum) {
      const [, , , , x] = rowOf(el.dataset.ngNum);
      const v = parseFloat(String(el.value).replace(',', '.'));
      if (Number.isFinite(v)) setValue(el.dataset.ngNum, Math.max(x.min, Math.min(x.max, v)));
      render();
    } else if (el.dataset.ngText) {
      setValue(el.dataset.ngText, el.value.replace(/["\\;\r\n]/g, '').trim());
    }
  }

  function toast(text) {
    const t = $('toast');
    if (!t) return;
    t.textContent = text;
    t.classList.add('show');
    clearTimeout(toast.timer);
    toast.timer = setTimeout(() => t.classList.remove('show'), 2600);
  }

  document.addEventListener('click', onClick);
  document.addEventListener('input', onInput);
  document.addEventListener('change', onChange);

  load();
  window.EpicNewGame = {
    open() {
      render();
      if (!maps) loadMaps();
    },
  };
})();
