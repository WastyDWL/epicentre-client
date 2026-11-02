// Epicentre main menu. Runs inside the client's browser (window.nextclient) and, for
// previews, in a normal browser with sample data.
(() => {
  const SERVER = '45.95.31.157:27115';
  const SITE = 'https://epicentre-cs.ru';
  const nc = window.nextclient;
  const mm = nc && nc.matchmaking;
  const preview = !nc;

  const $ = (id) => document.getElementById(id);
  const esc = (s) => String(s).replace(/[&<>"]/g, (c) => ({ '&': '&amp;', '<': '&lt;', '>': '&gt;', '"': '&quot;' })[c]);

  function toast(text) {
    const t = $('toast');
    t.textContent = text;
    t.classList.add('show');
    clearTimeout(toast.timer);
    toast.timer = setTimeout(() => t.classList.remove('show'), 2200);
  }

  function connect(address) {
    if (preview) return toast('connect ' + address);
    nc.exec('connect ' + address);
  }

  function openSite(path) {
    // The site forbids framing, so it opens in this view; the client adds a "back to menu" button.
    location.href = SITE + (path || '/');
  }

  // ---- sample data for previews outside the game ----
  const sample = {
    info: { hostname: 'Эпицентр Арена Страйка 18+ | БЕСПЛАТНЫЙ VIP |', map: 'cs_mansion', playersOnline: 14, playersMax: 32, address: SERVER },
    players: ['}I{eI{@1988', 'JIyHb', 'smeliy', 'Valter59', 'Hy KaMoH', 'Эцио', 'Hope_Trader', 'WOL3'].map((name, i) => ({ name, score: 40 - i * 4 })),
    list: [
      { hostname: 'Эпицентр Арена Страйка 18+ | БЕСПЛАТНЫЙ VIP |', map: 'cs_mansion', playersOnline: 14, playersMax: 32, address: SERVER },
      { hostname: 'Classic Public de_dust2', map: 'de_dust2', playersOnline: 21, playersMax: 32, address: '1.2.3.4:27015' },
      { hostname: 'Deathrun Winter', map: 'deathrun_snow', playersOnline: 7, playersMax: 30, address: '5.6.7.8:27015' },
      { hostname: 'Zombie Plague', map: 'zm_dust', playersOnline: 3, playersMax: 32, address: '9.9.9.9:27015' },
    ],
  };

  const getInfo = (addr) => (mm ? mm.getServerInfo(addr) : Promise.resolve(sample.info));
  const getPlayers = (addr) => (mm ? mm.getPlayersInfo(addr) : Promise.resolve(sample.players));
  function getList(kind) {
    if (!mm) return Promise.resolve(sample.list);
    if (kind === 'favorites') return mm.getFavoriteServers();
    if (kind === 'history') return mm.getHistoryServers();
    return mm.getInternetServers ? mm.getInternetServers() : mm.getHistoryServers();
  }

  // ---- home ----
  async function refreshHome() {
    $('hero-addr').textContent = SERVER;
    try {
      const s = await getInfo(SERVER);
      $('hero-name').textContent = s.hostname;
      $('hero-map-name').textContent = s.map;
      $('hero-online').textContent = s.playersOnline;
      $('hero-max').textContent = s.playersMax;
      $('hero-bots').textContent = s.botsOnline ? ` · +${s.botsOnline} ботов` : '';
      $('hero-bar').style.width = Math.min(100, (100 * s.playersOnline) / Math.max(1, s.playersMax)) + '%';
      $('hero-dot').className = 'dot on';
      $('hero-state').textContent = 'Сервер онлайн';
    } catch {
      $('hero-dot').className = 'dot off';
      $('hero-state').textContent = 'Сервер не отвечает';
    }
    try {
      const players = (await getPlayers(SERVER)).sort((a, b) => b.score - a.score).slice(0, 12);
      $('players').innerHTML = players.length
        ? players.map((p) => `<li><span>${esc(p.name)}</span><span>${p.score}</span></li>`).join('')
        : '<li class="muted">Сейчас никого нет — заходите первым!</li>';
    } catch {
      $('players').innerHTML = '<li class="muted">Список игроков недоступен</li>';
    }
  }

  // ---- servers ----
  let listKind = 'internet';
  // Per list: address -> server. Filled while a query runs (nextclient.matchmaking 'servers'
  // batches) and completed by the query's final answer.
  const lists = { internet: new Map(), favorites: new Map(), history: new Map() };
  const loading = { internet: 0, favorites: 0, history: 0 };
  const filters = { notEmpty: false, notFull: false, noPassword: false, noBots: false };
  const favorites = new Set();  // addresses in Steam favorites (kept in sync with lists.favorites)
  const ADDRESS = /^(\d{1,3}\.){3}\d{1,3}(:\d{1,5})?$|^[a-z0-9.-]+\.[a-z]{2,}(:\d{1,5})?$/i;

  // NextClient lists are [{address, info}] (info null for servers that did not answer);
  // sample data and getServerInfo are flat. Normalise to flat items.
  function normalize(item) {
    if (item && 'info' in item) {
      const i = item.info || {};
      return { ...i, address: item.address, offline: !item.info };
    }
    return item;
  }

  function merge(kind, items) {
    const map = lists[kind];
    if (kind === 'favorites') for (const raw of items) if (raw && raw.address) favorites.add(raw.address);
    for (const raw of items) {
      const s = normalize(raw);
      if (!s || !s.address) continue;
      const old = map.get(s.address);
      if (!old || old.offline || !s.offline) map.set(s.address, s);
    }
  }

  let renderQueued = false;
  function scheduleRender() {
    if (renderQueued) return;
    renderQueued = true;
    requestAnimationFrame(() => {
      renderQueued = false;
      renderList();
    });
  }

  function passes(s) {
    if (s.offline) return !filters.notEmpty && !filters.notFull;
    if (filters.notEmpty && !(s.playersOnline > 0)) return false;
    if (filters.notFull && !(s.playersOnline + (s.botsOnline || 0) < s.playersMax)) return false;
    if (filters.noPassword && s.isPasswordProtected) return false;
    if (filters.noBots && s.botsOnline > 0) return false;
    return true;
  }

  function pingCell(p) {
    if (p == null) return '<div class="ping">—</div>';
    return `<div class="ping ${p < 60 ? 'good' : p > 150 ? 'bad' : ''}">${p}</div>`;
  }

  function renderList() {
    const all = [...lists[listKind].values()];
    const q = $('search').value.trim().toLowerCase();
    const rows = all
      .filter(passes)
      .filter((s) => !q || [s.hostname, s.map, s.address].some((v) => String(v || '').toLowerCase().includes(q)))
      .sort((a, b) => (b.address === SERVER) - (a.address === SERVER) || (b.playersOnline || 0) - (a.playersOnline || 0));
    $('add-fav').classList.toggle('show', listKind === 'favorites');
    const busy = loading[listKind] > 0;
    $('refresh').disabled = busy;
    $('count').textContent = all.length
      ? `Показано ${rows.length} из ${all.length}${busy ? ' · загрузка…' : ''}`
      : '';
    if (!rows.length) {
      $('list').innerHTML = `<div class="empty">${busy ? 'Загрузка списка…' : all.length ? 'Под фильтры ничего не подходит' : 'Серверы не найдены'}</div>`;
      return;
    }
    $('list').innerHTML = rows.map((s) => `
        <div class="row${s.address === SERVER ? ' ours' : ''}${s.offline ? ' offline' : ''}">
          <div><div class="name">${esc(s.offline ? 'Сервер не отвечает' : s.hostname)}</div><div class="sub">${esc(s.address)}</div></div>
          <div class="map">${esc(s.map || '—')}</div>
          <div class="pl">${s.offline ? '—' : `<b>${s.playersOnline}</b> / ${s.playersMax}${s.botsOnline ? ` <span class="bots">+${s.botsOnline} бот.</span>` : ''}`}</div>
          ${pingCell(s.offline ? null : s.ping)}
          <button class="fav${favorites.has(s.address) ? ' on' : ''}" data-fav="${esc(s.address)}"
            title="${favorites.has(s.address) ? 'Убрать из избранного' : 'Добавить в избранное'}">${favorites.has(s.address) ? '★' : '☆'}</button>
          <button class="go" data-connect="${esc(s.address)}">Играть</button>
        </div>`).join('');
  }

  async function loadList(kind = listKind) {
    if (loading[kind]) return;
    loading[kind]++;
    lists[kind].clear();
    if (kind === listKind) renderList();
    try {
      merge(kind, await getList(kind));
    } catch {
      // keep whatever arrived as batches
    }
    loading[kind]--;
    if (kind === listKind) renderList();
  }

  if (mm && mm.addEventListener) {
    mm.addEventListener('servers', (e) => {
      const d = e.detail || {};
      if (!lists[d.list] || !Array.isArray(d.items)) return;
      merge(d.list, d.items);
      if (d.list === listKind) scheduleRender();
    });
  }

  async function setFavorite(address, on) {
    if (on) {
      favorites.add(address);
      const known = lists.internet.get(address) || lists.history.get(address);
      lists.favorites.set(address, known || { address, hostname: address, offline: true });
    } else {
      favorites.delete(address);
      lists.favorites.delete(address);
    }
    renderList();
    toast(on ? 'Добавлено в избранное' : 'Убрано из избранного');
    if (preview) return;
    try {
      await (on ? mm.addFavoriteServer(address) : mm.removeFavoriteServer(address));
    } catch {
      toast('Не удалось изменить избранное');
    }
  }

  $('add-fav').addEventListener('submit', (e) => {
    e.preventDefault();
    const input = $('add-fav-addr');
    let addr = input.value.trim();
    if (!ADDRESS.test(addr)) {
      input.classList.add('bad');
      return toast('Введите адрес в виде IP:порт');
    }
    if (!/:\d+$/.test(addr)) addr += ':27015';
    input.classList.remove('bad');
    input.value = '';
    setFavorite(addr, true);
    // ask the server for its details so the row fills in
    getInfo(addr).then((info) => {
      if (favorites.has(addr)) lists.favorites.set(addr, { ...info, address: addr, offline: false });
      renderList();
    }, () => {});
  });

  // ---- game actions (the vgui game menu is hidden) ----
  function gameCommand(name) {
    if (preview) return toast('gameUi.command(' + name + ')');
    nc.gameUi.command(name).catch(() => toast('Команда недоступна'));
  }

  function renderRail() {
    const host = nc ? nc.connectedHost : null;
    const inGame = !!host;
    const b = (cmd, label, cls = '') => `<button class="tab ${cls}" data-cmd="${cmd}">${label}</button>`;
    $('rail').innerHTML = (inGame
      ? b('Resume', '▶ В игру', 'primary') + b('Players', 'Игроки') + b('Disconnect', 'Отключиться', 'danger')
      : b('NewGame', 'Новая игра')) +
      b('Options', 'Настройки') + b('Quit', 'Выход', 'danger');
  }

  // ---- wiring ----
  document.addEventListener('click', (e) => {
    const t = e.target.closest('button, .addr');
    if (!t) return;
    if (t.id === 'connect') connect(SERVER);
    else if (t.dataset.cmd) gameCommand(t.dataset.cmd);
    else if (t.dataset.connect) connect(t.dataset.connect);
    else if (t.dataset.fav) setFavorite(t.dataset.fav, !favorites.has(t.dataset.fav));
    else if (t.dataset.site) openSite(t.dataset.site);
    else if (t.dataset.action === 'site') openSite('/');
    else if (t.classList.contains('addr')) {
      navigator.clipboard?.writeText(SERVER);
      toast('Адрес скопирован');
    } else if (t.dataset.tab) {
      document.querySelectorAll('.tab').forEach((b) => b.classList.toggle('active', b === t));
      document.querySelectorAll('.page').forEach((p) => p.classList.toggle('active', p.id === 'page-' + t.dataset.tab));
      if (t.dataset.tab === 'servers') {
        if (!lists[listKind].size && !loading[listKind]) loadList();
        else renderList();
      }
    } else if (t.dataset.list) {
      listKind = t.dataset.list;
      document.querySelectorAll('.chip').forEach((b) => b.classList.toggle('active', b === t));
      if (!lists[listKind].size && !loading[listKind]) loadList();
      else renderList();
    }
  });
  $('search').addEventListener('input', renderList);
  $('refresh').addEventListener('click', () => loadList());
  document.querySelectorAll('[data-filter]').forEach((box) =>
    box.addEventListener('change', () => {
      filters[box.dataset.filter] = box.checked;
      renderList();
    }));

  const name = nc ? nc.getCvarValue('name') : 'Игрок';
  if (name) $('profile').innerHTML = 'Вы играете как <b>' + esc(name) + '</b>';

  refreshHome();
  loadList('internet');  // prefetch: the server list is ready by the time the tab is opened
  loadList('favorites');  // also tells which servers get a filled star
  setInterval(refreshHome, 30000);
  renderRail();
  if (nc && nc.gameUi) nc.gameUi.addEventListener('show', () => {
    refreshHome();
    renderRail();
  });
  if (nc) ['connect', 'disconnect', 'putinserver'].forEach((ev) => nc.addEventListener(ev, renderRail));
})();
