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
  let listCache = [];
  const filters = { notEmpty: false, notFull: false, noPassword: false, noBots: false };

  // NextClient lists are [{address, info}] (info null for servers that did not answer);
  // sample data and getServerInfo are flat. Normalise to flat items.
  function normalize(item) {
    if (item && 'info' in item) {
      const i = item.info || {};
      return { ...i, address: item.address, offline: !item.info };
    }
    return item;
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
    const q = $('search').value.trim().toLowerCase();
    const rows = listCache
      .filter(passes)
      .filter((s) => !q || [s.hostname, s.map, s.address].some((v) => String(v || '').toLowerCase().includes(q)))
      .sort((a, b) => (b.address === SERVER) - (a.address === SERVER) || (b.playersOnline || 0) - (a.playersOnline || 0));
    $('count').textContent = listCache.length ? `Показано ${rows.length} из ${listCache.length}` : '';
    $('list').innerHTML = rows.length
      ? rows.map((s) => `
        <div class="row${s.address === SERVER ? ' ours' : ''}${s.offline ? ' offline' : ''}">
          <div><div class="name">${esc(s.offline ? 'Сервер не отвечает' : s.hostname)}</div><div class="sub">${esc(s.address)}</div></div>
          <div class="map">${esc(s.map || '—')}</div>
          <div class="pl">${s.offline ? '—' : `<b>${s.playersOnline}</b> / ${s.playersMax}${s.botsOnline ? ` <span class="bots">+${s.botsOnline} бот.</span>` : ''}`}</div>
          ${pingCell(s.offline ? null : s.ping)}
          <button class="go" data-connect="${esc(s.address)}">Играть</button>
        </div>`).join('')
      : `<div class="empty">${listCache.length ? 'Под фильтры ничего не подходит' : 'Серверы не найдены'}</div>`;
  }

  let loadToken = 0;
  async function loadList() {
    const token = ++loadToken;
    $('refresh').disabled = true;
    $('list').innerHTML = '<div class="empty">Загрузка списка…</div>';
    $('count').textContent = '';
    let items = [];
    try {
      items = (await getList(listKind)).map(normalize);
    } catch {
      items = [];
    }
    if (token !== loadToken) return;  // a newer refresh started meanwhile
    listCache = items;
    $('refresh').disabled = false;
    renderList();
  }

  // ---- wiring ----
  document.addEventListener('click', (e) => {
    const t = e.target.closest('button, .addr');
    if (!t) return;
    if (t.id === 'connect') connect(SERVER);
    else if (t.dataset.connect) connect(t.dataset.connect);
    else if (t.dataset.site) openSite(t.dataset.site);
    else if (t.dataset.action === 'site') openSite('/');
    else if (t.classList.contains('addr')) {
      navigator.clipboard?.writeText(SERVER);
      toast('Адрес скопирован');
    } else if (t.dataset.tab) {
      document.querySelectorAll('.tab').forEach((b) => b.classList.toggle('active', b === t));
      document.querySelectorAll('.page').forEach((p) => p.classList.toggle('active', p.id === 'page-' + t.dataset.tab));
      if (t.dataset.tab === 'servers' && !listCache.length) loadList();
    } else if (t.dataset.list) {
      listKind = t.dataset.list;
      document.querySelectorAll('.chip').forEach((b) => b.classList.toggle('active', b === t));
      loadList();
    }
  });
  $('search').addEventListener('input', renderList);
  $('refresh').addEventListener('click', loadList);
  document.querySelectorAll('[data-filter]').forEach((box) =>
    box.addEventListener('change', () => {
      filters[box.dataset.filter] = box.checked;
      renderList();
    }));

  const name = nc ? nc.getCvarValue('name') : 'Игрок';
  if (name) $('profile').innerHTML = 'Вы играете как <b>' + esc(name) + '</b>';

  refreshHome();
  setInterval(refreshHome, 30000);
  if (nc && nc.gameUi) nc.gameUi.addEventListener('show', refreshHome);
})();
