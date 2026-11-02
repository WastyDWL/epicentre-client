// Battle pass page (winter season of the site's xmas_game module), CS2 pass layout: the level and
// XP on top, the two reward tracks (free / premium) as a horizontal strip of levels, purchases on
// the right. Data and actions go through EpicAccount (api/client); the run itself is the site's
// game page, opened signed in through SSO.
(() => {
  const ACC = () => window.EpicAccount;
  const IMG = 'https://epicentre-cs.ru/modules_extra/xmas_game/templates/slim/img/';
  const KIND_ICON = { money: 'bp_money.svg', coins: 'bp_coins.svg', tickets: 'bp_tickets.svg', service: 'bp_service.svg', rcon: 'bp_service.svg' };
  const STATUS = {
    claimable: 'Забрать', done: 'Получено', locked: 'Закрыто', premium_locked: 'Нужен премиум',
    pending_online: 'Ждёт входа на сервер', failed: 'Ошибка — напишите админу',
  };
  const METHODS = ['Ник + пароль', 'STEAM ID', 'STEAM ID + пароль'];

  const $ = (id) => document.getElementById(id);
  const esc = (s) => String(s == null ? '' : s).replace(/[&<>"']/g, (c) => ({ '&': '&amp;', '<': '&lt;', '>': '&gt;', '"': '&quot;', "'": '&#39;' })[c]);
  const rub = (v) => (Math.round(Number(v || 0) * 100) / 100).toLocaleString('ru-RU') + ' ₽';
  const plural = (n, one, few, many) => {
    const a = n % 10, b = n % 100;
    return a === 1 && b !== 11 ? one : a >= 2 && a <= 4 && (b < 12 || b > 14) ? few : many;
  };

  let data = null;    // last 'bp' answer
  let loading = false;

  // the last pass, drawn at once on open while the site is asked again
  const CACHE = 'epic.pass';
  const cache = {
    get() { try { return JSON.parse(localStorage.getItem(CACHE)); } catch { return null; } },
    set(v) { try { if (v) localStorage.setItem(CACHE, JSON.stringify(v)); else localStorage.removeItem(CACHE); } catch { /* none */ } },
  };
  let rewardsById = new Map();

  function toast(text) {
    const t = $('toast');
    if (!t) return;
    t.textContent = text;
    t.classList.add('show');
    clearTimeout(toast.timer);
    toast.timer = setTimeout(() => t.classList.remove('show'), 2600);
  }

  // ---- sample season for previews outside the client ----
  function sample() {
    const levels = [];
    const kinds = ['coins', 'tickets', 'money', 'service', 'rcon'];
    for (let n = 1; n <= 30; n++) {
      const st = (track) => (n <= 6 ? (track === 'premium' ? 'premium_locked' : n < 4 ? 'done' : 'claimable') : 'locked');
      levels.push({
        level: n, xp_cost: 300 + n * 2,
        free: n % 2 ? [{ id: n * 2, kind: kinds[n % 5], title: n % 5 === 3 ? 'VIP на 3 дня' : (n * 10) + ' монет', image: null, status: st('free'), binds: '1;1;0' }] : [],
        premium: [{ id: n * 2 + 1, kind: kinds[(n + 2) % 5], title: n % 3 ? 'Билеты ×2' : 'Админка на 7 дней', image: null, status: st('premium'), binds: '1;1;1' }],
      });
    }
    return {
      ok: true, balance: 150,
      season: { title: 'Зима 2026', starts: '01.11.2026', ends: '14.01.2027' },
      state: { level: 6, max_level: 100, xp: 1980, xp_level_start: 1830, xp_next: 315, tickets: 3, coins: 140, runs_left: 4, extra_left: 2,
        premium: false, packs: [[1, 15], [5, 60], [10, 100]], ext_price: 30, ext_runs: 2, premium_price: 299, ticket_coins: 80, season_open: true, season_ends: '14.01.2027' },
      levels,
    };
  }

  async function load() {
    if (loading || playing) return;
    loading = true;
    const acc = ACC();
    if (!acc.signedIn && !acc.preview) {
      loading = false;
      data = null;
      return render();
    }
    if (!data && !acc.preview) data = cache.get();
    if (data) render();
    else $('page-pass').innerHTML = '<div class="bp-empty">Загрузка батлпасса…</div>';
    const res = acc.preview ? sample() : await acc.call('bp');
    loading = false;
    if (!res.ok) {
      if (data) return toast(res.message || 'Не удалось обновить батлпасс');
      $('page-pass').innerHTML = `<div class="bp-empty">${esc(res.message || 'Не удалось загрузить батлпасс')}</div>`;
      return;
    }
    data = res;
    if (!acc.preview) cache.set(res);
    render();
  }

  // ---- rendering ----
  function rewardCard(r, level) {
    const img = r.image || IMG + (KIND_ICON[r.kind] || 'bp_service.svg');
    const label = STATUS[r.status] || STATUS.locked;
    return `<button class="bp-reward is-${esc(r.status)}" data-reward="${r.id}" ${r.status === 'claimable' ? '' : 'tabindex="-1"'}>
        <img src="${esc(img)}" alt="" loading="lazy">
        <span class="bp-reward-title">${esc(r.title)}</span>
        <span class="bp-reward-status">${r.status === 'locked' ? 'Уровень ' + level : label}</span>
      </button>`;
  }

  function render() {
    if (playing) return; // the game holds the tab
    const page = $('page-pass');
    const oldTrack = $('bp-track');
    const keepScroll = oldTrack ? oldTrack.scrollLeft : null;
    if (!data) {
      page.innerHTML = `<div class="bp-gate">
          <div class="bp-gate-art">❄</div>
          <h2>Зимний батлпасс</h2>
          <p>Войдите в аккаунт сайта epicentre-cs.ru: уровни, награды и новогодняя игра будут прямо в клиенте.</p>
          <button class="connect" data-account="login">Войти</button>
        </div>`;
      return;
    }
    if (!data.season) {
      page.innerHTML = '<div class="bp-empty">Сейчас сезон батлпасса не идёт.</div>';
      return;
    }
    const s = data.state;
    rewardsById = new Map();
    data.levels.forEach((l) => ['free', 'premium'].forEach((t) => l[t].forEach((r) => rewardsById.set(r.id, { ...r, level: l.level }))));
    const inLevel = Math.max(0, s.xp - s.xp_level_start);
    const pct = s.xp_next ? Math.min(100, (100 * inLevel) / s.xp_next) : 100;
    const claimable = [...rewardsById.values()].filter((r) => r.status === 'claimable').length;

    page.innerHTML = `
      <header class="bp-head">
        <div class="bp-season">
          <span class="bp-tag">Сезон</span>
          <h2>${esc(data.season.title || 'Зимний батлпасс')}</h2>
          <span class="bp-dates">до ${esc(data.season.ends)}${s.season_open ? '' : ' · сезон закрыт'}</span>
        </div>
        <div class="bp-level">
          <div class="bp-level-num"><small>Уровень</small><b>${s.level}</b><small>из ${s.max_level}</small></div>
          <div class="bp-xp">
            <div class="bp-xp-bar"><i style="width:${pct}%"></i></div>
            <span>${s.xp_next ? `${inLevel} / ${s.xp_next} XP до уровня ${s.level + 1}` : 'Максимальный уровень'}</span>
          </div>
        </div>
        <div class="bp-res">
          <div><b>${s.tickets}</b><small>${plural(s.tickets, 'билет', 'билета', 'билетов')}</small></div>
          <div><b>${s.coins}</b><small>${plural(s.coins, 'монета', 'монеты', 'монет')}</small></div>
          <div><b>${s.runs_left}</b><small>забегов с XP</small></div>
          <div><b>${rub(data.balance)}</b><small>на балансе</small></div>
        </div>
      </header>

      <div class="bp-main">
        <section class="bp-track-wrap">
          <div class="bp-rows-head">
            <span class="bp-row-label">Бесплатно</span>
            <span class="bp-row-label premium">Премиум${s.premium ? ' ✓' : ''}</span>
          </div>
          <div class="bp-track" id="bp-track">
            ${data.levels.map((l) => `
              <div class="bp-col${l.level <= s.level ? ' reached' : ''}${l.level === s.level ? ' current' : ''}" data-level="${l.level}">
                <div class="bp-cell">${l.free.map((r) => rewardCard(r, l.level)).join('') || '<span class="bp-none"></span>'}</div>
                <div class="bp-lvl">${l.level}</div>
                <div class="bp-cell premium${s.premium ? '' : ' locked'}">${l.premium.map((r) => rewardCard(r, l.level)).join('') || '<span class="bp-none"></span>'}</div>
              </div>`).join('')}
          </div>
          <div class="bp-track-nav">
            <button class="bp-nav" data-bp="prev" aria-label="Назад">‹</button>
            <span>${claimable ? `Можно забрать: <b>${claimable}</b>` : 'Играйте, чтобы открыть новые награды'}</span>
            <button class="bp-nav" data-bp="next" aria-label="Вперёд">›</button>
          </div>
        </section>

        <aside class="bp-side">
          <button class="connect bp-play" data-bp="play">▶ Играть в новогоднюю игру</button>
          <p class="muted small">Забег стоит билет и даёт опыт. ${s.runs_left > 0 ? `Сегодня ещё ${s.runs_left} ${plural(s.runs_left, 'забег', 'забега', 'забегов')} с опытом.` : 'Лимит забегов с опытом на сегодня исчерпан.'}</p>
          ${s.premium ? '<div class="bp-premium-on">★ Премиум активен</div>' : `
          <div class="bp-premium">
            <b>Премиум-дорожка</b>
            <span>Вторая линия наград на весь сезон</span>
            <button class="bp-buy gold" data-bp="premium">Купить за ${rub(s.premium_price)}</button>
          </div>`}
          <div class="bp-shop">
            <b>Билеты</b>
            <div class="bp-packs">
              ${(s.packs || []).map(([n, price]) => `<button class="bp-buy" data-bp="tickets" data-count="${n}">${n} ${plural(n, 'билет', 'билета', 'билетов')}<small>${rub(price)}</small></button>`).join('')}
            </div>
            <button class="bp-buy ghost" data-bp="exchange" data-count="1" ${s.coins >= s.ticket_coins ? '' : 'disabled'}>Обменять ${s.ticket_coins} монет на билет</button>
            ${s.extra_left > 0 ? `<button class="bp-buy ghost" data-bp="extend">+${s.ext_runs} ${plural(s.ext_runs, 'забег', 'забега', 'забегов')} с опытом · ${rub(s.ext_price)}</button>` : ''}
          </div>
        </aside>
      </div>`;

    // the current level in view
    const cur = page.querySelector('.bp-col.current') || page.querySelector('.bp-col');
    const track = $('bp-track');
    if (keepScroll != null) track.scrollLeft = keepScroll;
    else if (cur) track.scrollLeft = Math.max(0, cur.offsetLeft - track.offsetLeft - 2 * cur.offsetWidth);
  }

  // ---- actions ----
  async function buy(kind, count) {
    const acc = ACC();
    const s = data.state;
    const what = {
      premium: `премиум-дорожку за ${rub(s.premium_price)}`,
      tickets: `${count} ${plural(count, 'билет', 'билета', 'билетов')}`,
      exchange: `билет за ${s.ticket_coins} монет`,
      extend: `ещё ${s.ext_runs} забега с опытом за ${rub(s.ext_price)}`,
    }[kind];
    if (!(await ask(`Купить ${what}?`, kind === 'exchange' ? 'Обменять' : 'Купить'))) return;
    if (acc.preview) return toast('Покупка: ' + what);
    const res = await acc.call('bp_buy', { kind, count: count || 0 });
    toast(res.message || (res.ok ? 'Готово' : 'Не удалось купить'));
    if (res.ok) {
      acc.refresh();
      load();
    }
  }

  // ---- the game, embedded in this tab ----
  // The native build (epicentre_games/xmas) runs in its own window that the game keeps over
  // #bp-stage; without it (not installed) the site's page opens instead.
  let playing = false;
  let pollTimer = null;
  const stageBox = () => $('bp-stage').getBoundingClientRect();

  function renderStage() {
    $('page-pass').innerHTML = `
      <div class="bp-game-bar">
        <button class="bp-nav wide" data-bp="leave">← К батлпассу</button>
        <b>Новогодняя игра</b>
        <span class="bp-live" id="bp-live">${data && data.state ? liveText(data.state) : ''}</span>
        <span class="muted small">Пробел — прыжок · пауза — кнопка ⏸ в игре</span>
      </div>
      <div class="bp-stage" id="bp-stage"><span>Загружаем игру…</span></div>`;
  }

  // the strip over the game: the pass as the runs change it (bp_state, polled while playing)
  function liveText(st) {
    const inLevel = Math.max(0, st.xp - st.xp_level_start);
    return `Уровень <b>${st.level}</b> · ${st.xp_next ? `${inLevel} / ${st.xp_next} XP` : 'макс.'} · `
      + `${st.tickets} ${plural(st.tickets, 'билет', 'билета', 'билетов')} · ${st.runs_left} ${plural(st.runs_left, 'забег', 'забега', 'забегов')} с XP`;
  }

  async function pollState() {
    const res = await ACC().call('bp_state');
    if (!res.ok || !res.state || !playing) return;
    const before = data && data.state;
    if (data) {
      data.state = res.state;
      data.balance = res.balance;
    }
    const el = $('bp-live');
    if (!el) return;
    el.innerHTML = liveText(res.state);
    if (before && (res.state.xp !== before.xp || res.state.level !== before.level)) {
      el.classList.remove('bump');
      void el.offsetWidth;
      el.classList.add('bump');
      if (res.state.level > before.level) toast(`Уровень ${res.state.level}! Новые награды в батлпассе`);
    }
  }

  async function play() {
    const acc = ACC();
    const nc = window.nextclient;
    if (acc.preview || !nc.account.launchGame) return acc.openSite('/xmas_game');
    const res = await acc.call('game_token');
    if (!res.ok) return toast(res.message || 'Не удалось запустить игру');
    playing = true;
    $('page-pass').classList.add('playing');
    renderStage();
    try {
      await nc.account.launchGame(res.token, res.login, stageBox());
    } catch {
      leave(false);
      return acc.openSite('/xmas_game');
    }
    clearInterval(pollTimer);
    let tick = 0;
    pollTimer = setInterval(async () => {
      let running = false;
      try { running = await nc.account.gameRunning(); } catch { /* treat as closed */ }
      if (!running && playing) return leave(false);
      if (++tick % 3 === 0) pollState();
    }, 1000);
  }

  // back to the pass; closeGame: the game is still running (the button, another tab)
  function leave(closeGame = true) {
    if (!playing) return;
    playing = false;
    clearInterval(pollTimer);
    if (closeGame) window.nextclient.account.closeGame().catch(() => {});
    $('page-pass').classList.remove('playing');
    render();
    load(); // runs changed the pass
  }

  // The game window gets no input of its own (the game must keep the keyboard focus): keys and the
  // mouse over the stage are forwarded to it.
  const send = (line) => window.nextclient.account.gameInput(line).catch(() => {});
  const MOUSE = { down: [0x201, 0x204, 0x207], up: [0x202, 0x205, 0x208] }; // WM_L/R/M BUTTON DOWN / UP
  const buttonsFlags = (e) => (e.buttons & 1 ? 1 : 0) | (e.buttons & 2 ? 2 : 0) | (e.buttons & 4 ? 0x10 : 0); // MK_*
  const stagePoint = (e) => {
    const r = stageBox(), k = window.devicePixelRatio || 1;
    return [Math.round((e.clientX - r.left) * k), Math.round((e.clientY - r.top) * k)];
  };
  const wmButton = (b) => (b === 2 ? 1 : b === 1 ? 2 : 0); // DOM button -> index in MOUSE

  ['keydown', 'keyup'].forEach((type) => document.addEventListener(type, (e) => {
    if (!playing || !e.keyCode) return;
    e.preventDefault();
    send(`k ${type === 'keydown' ? 1 : 0} ${e.keyCode} ${e.repeat ? 1 : 0}`);
  }, true));

  let moveQueued = null;
  document.addEventListener('mousemove', (e) => {
    if (!playing || !e.target.closest('#bp-stage')) return;
    const first = !moveQueued;
    moveQueued = e;
    if (first) requestAnimationFrame(() => {
      const ev = moveQueued;
      moveQueued = null;
      const [x, y] = stagePoint(ev);
      send(`m 512 ${x} ${y} ${buttonsFlags(ev)}`); // WM_MOUSEMOVE
    });
  });
  ['mousedown', 'mouseup'].forEach((type) => document.addEventListener(type, (e) => {
    if (!playing || !e.target.closest('#bp-stage')) return;
    e.preventDefault();
    const [x, y] = stagePoint(e);
    send(`m ${MOUSE[type === 'mousedown' ? 'down' : 'up'][wmButton(e.button)]} ${x} ${y} ${buttonsFlags(e)}`);
  }));
  document.addEventListener('contextmenu', (e) => { if (playing && e.target.closest('#bp-stage')) e.preventDefault(); });

  window.addEventListener('resize', () => {
    if (playing && $('bp-stage')) window.nextclient.account.setGameRect(stageBox()).catch(() => {});
  });
  window.addEventListener('pagehide', () => leave());

  // the pass stays current while its tab is open (claims, runs on the site, server coins)
  setInterval(() => {
    const page = $('page-pass');
    if (!playing && page && page.classList.contains('active') && ACC().signedIn && !document.querySelector('.modal-wrap')) load();
  }, 20000);

  async function claim(id, extra = {}) {
    const acc = ACC();
    if (acc.preview) return toast('Награда получена');
    const res = await acc.call('bp_claim', { reward: id, ...extra });
    if (!res.ok && res.need === 'steam_id') return openClaimForm(rewardsById.get(id), res.message);
    closeModal();
    toast(res.message || (res.ok ? 'Награда получена' : 'Не удалось получить награду'));
    if (res.ok) {
      acc.refresh();
      load();
    }
  }

  function openClaim(r) {
    if (!r || r.status !== 'claimable') return;
    if (r.kind === 'service' || r.kind === 'rcon') return openClaimForm(r);
    claim(r.id);
  }

  // in-page confirmation: the client's browser shows no native dialogs
  function ask(text, yes) {
    closeModal();
    return new Promise((resolve) => {
      const wrap = document.createElement('div');
      wrap.className = 'modal-wrap';
      wrap.id = 'bp-modal';
      wrap.innerHTML = `<div class="modal login"><h2>${esc(text)}</h2>
          <div class="modal-actions"><button class="bp-buy ghost" data-ask="0">Отмена</button><button class="connect" data-ask="1">${esc(yes)}</button></div></div>`;
      wrap.addEventListener('click', (e) => {
        const b = e.target.closest('[data-ask]');
        if (!b && e.target !== wrap) return;
        e.stopPropagation();
        wrap.remove();
        resolve(!!b && b.dataset.ask === '1');
      });
      document.body.appendChild(wrap);
    });
  }

  function closeModal() {
    const m = $('bp-modal');
    if (m) m.remove();
  }

  // Service rewards need the way the privilege is bound on the server; rcon ones a SteamID
  function openClaimForm(r, error) {
    closeModal();
    const user = ACC().user || {};
    let body;
    if (r.kind === 'service') {
      const binds = String(r.binds || '').split(';');
      const methods = METHODS.map((name, i) => (binds[i] === '1' ? i + 1 : 0)).filter(Boolean);
      if (!methods.length) {
        toast('На этом сервере выдача услуг выключена — напишите администратору');
        return;
      }
      body = `<div class="bp-methods">${methods.map((m, i) => `<label><input type="radio" name="bp-method" value="${m}" ${i ? '' : 'checked'}> ${METHODS[m - 1]}</label>`).join('')}</div>
        <label data-for="1">Ник в игре<input id="bp-nick" maxlength="32" value="${esc(user.nick || (window.nextclient ? window.nextclient.getCvarValue('name') : ''))}"></label>
        <label data-for="2 3">STEAM ID<input id="bp-steam" maxlength="32" placeholder="STEAM_0:1:12345678" value="${esc(user.steam_id || '')}"></label>
        <label data-for="1 3">Пароль для входа на сервер<input id="bp-pass" type="password" maxlength="32" autocomplete="new-password"></label>
        <p class="muted small">Пароль — это setinfo-ключ при заходе на сервер. Сайт его не сохраняет.</p>`;
    } else {
      body = `<label>STEAM ID<input id="bp-steam" maxlength="32" placeholder="STEAM_0:1:12345678" value="${esc(user.steam_id || '')}"></label>
        <label class="row"><input type="checkbox" id="bp-save" checked> Запомнить STEAM ID в профиле</label>
        <p class="muted small">Награда придёт, когда вы зайдёте на сервер с этим STEAM ID.</p>`;
    }
    const wrap = document.createElement('div');
    wrap.className = 'modal-wrap';
    wrap.id = 'bp-modal';
    wrap.innerHTML = `<form class="modal login" id="bp-claim-form">
        <button type="button" class="modal-x" data-bp="close" aria-label="Закрыть">×</button>
        <h2>${esc(r.title)}</h2>
        ${error ? `<p class="login-error">${esc(error)}</p>` : ''}
        ${body}
        <button class="connect" type="submit">Получить</button>
      </form>`;
    document.body.appendChild(wrap);
    const sync = () => {
      const m = (wrap.querySelector('input[name="bp-method"]:checked') || {}).value;
      wrap.querySelectorAll('[data-for]').forEach((el) => { el.hidden = !m || !el.dataset.for.split(' ').includes(m); });
    };
    wrap.querySelectorAll('input[name="bp-method"]').forEach((el) => el.addEventListener('change', sync));
    sync();
    $('bp-claim-form').addEventListener('submit', (e) => {
      e.preventDefault();
      const v = (id) => ($(id) ? $(id).value.trim() : '');
      if (r.kind === 'service') {
        claim(r.id, { method: (wrap.querySelector('input[name="bp-method"]:checked') || {}).value || '', nick: v('bp-nick'), steam_id: v('bp-steam'), pass: $('bp-pass') ? $('bp-pass').value : '' });
      } else {
        claim(r.id, { steam_id: v('bp-steam'), save_steam: $('bp-save') && $('bp-save').checked ? 1 : 0 });
      }
    });
  }

  document.addEventListener('click', (e) => {
    const reward = e.target.closest('.bp-reward');
    if (reward) return openClaim(rewardsById.get(Number(reward.dataset.reward)));
    const t = e.target.closest('[data-bp]');
    if (!t) {
      if (e.target.id === 'bp-modal') closeModal();
      return;
    }
    const track = $('bp-track');
    switch (t.dataset.bp) {
      case 'prev':
      case 'next':
        if (track) track.scrollBy({ left: (t.dataset.bp === 'prev' ? -1 : 1) * track.clientWidth * 0.8, behavior: 'smooth' });
        return;
      case 'play': return play();
      case 'leave': return leave();
      case 'close': return closeModal();
      default: return buy(t.dataset.bp, Number(t.dataset.count || 0));
    }
  });
  document.addEventListener('keydown', (e) => { if (e.key === 'Escape') closeModal(); });

  window.EpicPass = { open: load, reload: () => { data = null; return load(); }, leave };
  // sign-out drops the pass; a profile refresh (balance) does not reload it
  let lastUserId = null;
  ACC().onChange((user) => {
    const id = user ? user.id : null;
    if (id === lastUserId) return;
    lastUserId = id;
    data = null;
    if (!id) cache.set(null);
    const page = document.getElementById('page-pass');
    if (page && page.classList.contains('active')) load();
  });
})();
