// Epicentre buy menu: CS2 layout with only what CS 1.6 sells, at CS 1.6 prices. Shown by GameUI's
// buy panel over the game (BuyMenuBrowser); buys with the CS 1.6 buy aliases, the server checks
// money, team and buy zone itself. _epic_money / _epic_team come from client_mini.
(() => {
  const nc = window.nextclient;
  const preview = !nc;
  const T = 1, CT = 2;

  // glyphs of weapons.ttf (CS2weapons by SaViMoN); svg for what it has no glyph of
  const SVG = {
    shield: '<svg viewBox="0 0 24 24"><path d="M12 2 4 5v6c0 5 3.4 9.4 8 11 4.6-1.6 8-6 8-11V5l-8-3Zm0 3.2 5 1.9V11c0 3.6-2.2 6.8-5 8.1V5.2Z"/></svg>',
    ammo: '<svg viewBox="0 0 24 24"><path d="M5 9c0-3 1-6 2-7 1 1 2 4 2 7v11H5V9Zm5 0c0-3 1-6 2-7 1 1 2 4 2 7v11h-4V9Zm5 0c0-3 1-6 2-7 1 1 2 4 2 7v11h-4V9Z"/></svg>',
    nvg: '<svg viewBox="0 0 24 24"><path d="M3 9h18v3a3 3 0 0 1-3 3h-2.5a2 2 0 0 1-1.8-1.1L12 12l-1.7 1.9A2 2 0 0 1 8.5 15H6a3 3 0 0 1-3-3V9Zm3.5 1.5a1.5 1.5 0 1 0 0 3 1.5 1.5 0 0 0 0-3Zm11 0a1.5 1.5 0 1 0 0 3 1.5 1.5 0 0 0 0-3ZM8 6h8l1 2H7l1-2Z"/></svg>',
  };

  // team: only that side may buy it (CS 1.6 rules); price null: the server decides (ammo)
  const COLS = [
    { title: 'Снаряжение', items: [
      { cmd: 'vest', name: 'Кевлар', price: 650, glyph: 'B' },
      { cmd: 'vesthelm', name: 'Кевлар + шлем', price: 1000, glyph: 'C' },
      { cmd: 'defuser', name: 'Набор сапёра', price: 200, glyph: 'K', team: CT },
      { cmd: 'nvgs', name: 'Ночное видение', price: 1250, svg: 'nvg' },
      { cmd: 'shield', name: 'Тактический щит', price: 2200, svg: 'shield', team: CT },
      { cmd: 'primammo', name: 'Патроны: основное', price: null, svg: 'ammo' },
      { cmd: 'secammo', name: 'Патроны: пистолет', price: null, svg: 'ammo' },
    ] },
    { title: 'Пистолеты', items: [
      { cmd: 'glock', name: 'Glock-18', price: 400, glyph: 'T' },
      { cmd: 'usp', name: 'USP', price: 500, glyph: 'U' },
      { cmd: 'p228', name: 'P228', price: 600, glyph: 'j' },
      { cmd: 'deagle', name: 'Desert Eagle', price: 650, glyph: 'I' },
      { cmd: 'elites', name: 'Dual Elites', price: 800, glyph: 'L', team: T },
      { cmd: 'fn57', name: 'Five-SeveN', price: 750, glyph: 'O', team: CT },
    ] },
    { title: 'ПП и тяжёлое', items: [
      { cmd: 'mac10', name: 'MAC-10', price: 1400, glyph: 'a', team: T },
      { cmd: 'tmp', name: 'TMP', price: 1250, glyph: 'd', team: CT },
      { cmd: 'mp5', name: 'MP5 Navy', price: 1500, glyph: 'c' },
      { cmd: 'ump45', name: 'UMP-45', price: 1700, glyph: 'e' },
      { cmd: 'p90', name: 'P90', price: 2350, glyph: 'h' },
      { cmd: 'm3', name: 'M3 Super 90', price: 1700, glyph: 'g' },
      { cmd: 'xm1014', name: 'XM1014', price: 3000, glyph: 'v' },
      { cmd: 'm249', name: 'M249', price: 5750, glyph: 'f' },
    ] },
    { title: 'Винтовки', items: [
      { cmd: 'galil', name: 'Galil', price: 2000, glyph: 'S', team: T },
      { cmd: 'famas', name: 'FAMAS', price: 2250, glyph: 'M', team: CT },
      { cmd: 'ak47', name: 'AK-47', price: 2500, glyph: 'A', team: T },
      { cmd: 'm4a1', name: 'M4A1', price: 3100, glyph: 'Y', team: CT },
      { cmd: 'sg552', name: 'SG 552', price: 3500, glyph: 'R', team: T },
      { cmd: 'aug', name: 'AUG', price: 3500, glyph: 'D', team: CT },
      { cmd: 'scout', name: 'Scout', price: 2750, glyph: 'q' },
      { cmd: 'awp', name: 'AWP', price: 4750, glyph: 'E' },
      { cmd: 'g3sg1', name: 'G3/SG-1', price: 5000, glyph: 'm', team: T },
      { cmd: 'sg550', name: 'SG 550', price: 4200, glyph: 'F', team: CT },
    ] },
    { title: 'Гранаты', items: [
      { cmd: 'flash', name: 'Световая', price: 200, glyph: 'J' },
      { cmd: 'hegren', name: 'Осколочная', price: 300, glyph: 'Q' },
      { cmd: 'sgren', name: 'Дымовая', price: 300, glyph: 'p' },
    ] },
  ];

  // VGUIMenu id the server opened with -> the column to start in (0 none)
  const MENU_COL = { 28: 0, 29: 2, 30: 3, 31: 4, 32: 3, 33: 3, 34: 1 };

  const $ = (id) => document.getElementById(id);
  const cvar = (name, def) => {
    if (preview) return def;
    const v = nc.getCvarValue(name);
    return v == null || v === '' ? def : v;
  };

  let team = T, money = 0, active = 0, open = false, visible = [];

  function itemsFor(col) {
    return col.items.filter((it) => !it.team || it.team === team);
  }

  function render() {
    document.body.classList.toggle('ct', team === CT);
    $('side-logo').src = team === CT ? 'logo_ct.png' : 'logo_tr.png';
    $('side-team').textContent = team === CT ? 'Спецназ' : 'Террористы';
    visible = COLS.map(itemsFor);
    $('cols').innerHTML = COLS.map((col, ci) => `
      <div class="col${active === ci + 1 ? ' on' : ''}" data-col="${ci + 1}">
        <div class="col-head"><span class="n">${ci + 1}</span>${col.title}</div>
        ${visible[ci].map((it, ii) => `
          <button class="card" data-col="${ci}" data-item="${ii}">
            <span class="k">${ii + 1}</span>
            <span class="ico">${it.svg ? SVG[it.svg] : it.glyph}</span>
            <span class="meta"><span class="name">${it.name}</span><span class="price">${it.price == null ? '' : '$' + it.price}</span></span>
          </button>`).join('')}
      </div>`).join('');
    updateMoney();
  }

  function updateMoney() {
    $('money').textContent = money;
    $('side-money').textContent = '$' + money;
    document.querySelectorAll('.card').forEach((el) => {
      const it = visible[el.dataset.col][el.dataset.item];
      el.classList.toggle('poor', it.price != null && it.price > money);
    });
  }

  function setActive(col) {
    active = col;
    document.querySelectorAll('.col').forEach((el) => el.classList.toggle('on', Number(el.dataset.col) === col));
  }

  function flash(el, cls) {
    el.classList.remove('bought', 'denied');
    void el.offsetWidth;
    el.classList.add(cls);
  }

  function buy(ci, ii) {
    const it = visible[ci] && visible[ci][ii];
    if (!it) return;
    const el = document.querySelector(`.card[data-col="${ci}"][data-item="${ii}"]`);
    if (it.price != null && it.price > money) {
      if (el) flash(el, 'denied');
      return;
    }
    if (el) flash(el, 'bought');
    $('side-last').innerHTML = 'Куплено: <b>' + it.name + '</b>';
    if (!preview) nc.exec(it.cmd);
  }

  function close() {
    if (!open) return;
    if (!preview) nc.exec('epic_buymenu_close');
    else hide();
  }

  function hide() {
    open = false;
    $('buy').hidden = true;
  }

  function show(menu) {
    open = true;
    team = Number(cvar('_epic_team', T)) === CT ? CT : T;
    money = Number(cvar('_epic_money', preview ? 16000 : 0)) || 0;
    active = MENU_COL[menu] || 0;
    $('side-last').textContent = '';
    render();
    $('buy').hidden = false;
  }

  // money changes after every purchase; the server's answer arrives a moment later
  setInterval(() => {
    if (!open || preview) return;
    const m = Number(cvar('_epic_money', money)) || 0;
    const t = Number(cvar('_epic_team', team)) === CT ? CT : T;
    if (t !== team) {
      team = t;
      money = m;
      render();
    } else if (m !== money) {
      money = m;
      updateMoney();
    }
  }, 120);

  document.addEventListener('click', (e) => {
    const card = e.target.closest('.card');
    if (card) return buy(Number(card.dataset.col), Number(card.dataset.item));
    const col = e.target.closest('.col-head');
    if (col) setActive(Number(col.parentElement.dataset.col));
  });
  document.addEventListener('contextmenu', (e) => e.preventDefault());

  document.addEventListener('keydown', (e) => {
    if (!open) return;
    if (e.code === 'Escape' || e.code === 'KeyB') {
      e.preventDefault();
      return close();
    }
    const m = /^(?:Digit|Numpad)(\d)$/.exec(e.code);
    if (!m) return;
    e.preventDefault();
    const d = Number(m[1]);
    if (d === 0) return active ? setActive(0) : close();
    if (!active) {
      if (d <= COLS.length) setActive(d);
      return;
    }
    buy(active - 1, d - 1);
  });

  window.EpicBuy = { open: show, close: hide };

  // the panel may have been opened before this page finished loading
  const pending = Number(cvar('_epic_buy', preview ? 28 : 0));
  if (pending) show(pending);
})();
