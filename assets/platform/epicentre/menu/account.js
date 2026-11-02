// Epicentre site account in the menu: sign-in through the site's client API (api/client on
// epicentre-cs.ru), the profile chip in the top bar, and signed-in site pages through SSO.
// The token is kept by the game (nextclient.account, DPAPI); outside the client (previews) a
// sample account is shown and nothing is sent.
(() => {
  const SITE = 'https://epicentre-cs.ru';
  const nc = window.nextclient;
  const preview = !nc || !nc.account;

  const $ = (id) => document.getElementById(id);
  const esc = (s) => String(s == null ? '' : s).replace(/[&<>"']/g, (c) => ({ '&': '&amp;', '<': '&lt;', '>': '&gt;', '"': '&quot;', "'": '&#39;' })[c]);
  const money = (v) => (Math.round(Number(v || 0) * 100) / 100).toLocaleString('ru-RU') + ' ₽';

  let token = null;
  let user = null;
  const listeners = [];

  // The last profile, shown at once while the site is asked again (the page reloads on every
  // return from a site page). localStorage may be missing: then the page simply waits.
  const CACHE = 'epic.account.user';
  const cache = {
    get() { try { return JSON.parse(localStorage.getItem(CACHE)); } catch { return null; } },
    set(v) { try { if (v) localStorage.setItem(CACHE, JSON.stringify(v)); else localStorage.removeItem(CACHE); } catch { /* none */ } },
  };

  function emit() {
    if (!preview) cache.set(user);
    renderProfile();
    listeners.forEach((fn) => { try { fn(user); } catch (e) { console.error(e); } });
  }

  // A call to the site API. The game sends it (the page itself cannot reach the site) and adds the
  // saved token; resolves with the JSON answer ({ok, ...}); "auth" signs the client out.
  async function call(action, params = {}, withToken = true) {
    if (withToken && !token) return { ok: false, error: 'auth', message: 'Войдите в аккаунт' };
    const form = new URLSearchParams({ action, ...params }).toString();
    let r;
    try {
      r = await nc.account.request(form);
    } catch (e) {
      console.error('[account] request', action, e);
      return { ok: false, error: 'network', message: 'Клиент не смог отправить запрос: ' + (e && e.message ? e.message : e) };
    }
    if (!r || !r.status) return { ok: false, error: 'network', message: 'Сайт недоступен' + (r && r.error ? ` (${r.error})` : '') };
    let res;
    try {
      res = JSON.parse(r.body);
    } catch {
      // not JSON: a PHP error page, a hosting stub or an anti-bot check
      console.error('[account] answer', action, r.status, String(r.body).slice(0, 500));
      return { ok: false, error: 'network', message: `Сайт ответил не так (HTTP ${r.status}): ${String(r.body).replace(/<[^>]+>/g, ' ').replace(/\s+/g, ' ').trim().slice(0, 140)}` };
    }
    if (res && res.error === 'auth' && withToken) await signOut(false);
    return res || { ok: false, message: 'Пустой ответ сайта' };
  }

  async function signIn(login, password) {
    const res = await call('login', { login, password, device: 'Клиент Эпицентр' }, false);
    if (!res.ok) return res;
    token = res.token;
    user = res.user;
    await nc.account.setToken(token);
    emit();
    return res;
  }

  async function signOut(tellSite = true) {
    if (tellSite && token) await call('logout').catch(() => {});
    token = null;
    user = null;
    if (!preview) await nc.account.setToken('').catch(() => {});
    emit();
  }

  async function refresh() {
    if (!token) return null;
    const res = await call('me');
    if (res.ok) {
      user = res.user;
      emit();
    }
    return user;
  }

  // A site page in this view, already signed in when the account is (one-time SSO code)
  async function openSite(path = '/') {
    if (token && !preview) {
      const res = await call('sso', { to: path });
      if (res.ok && res.url) {
        location.href = res.url;
        return;
      }
    }
    location.href = SITE + path;
  }

  // ---- top bar chip ----
  function renderProfile() {
    const box = $('profile');
    if (!box) return;
    if (!user) {
      const name = nc ? nc.getCvarValue('name') : 'Игрок';
      box.innerHTML = `<span class="pf-game">Вы играете как <b>${esc(name || 'Игрок')}</b></span>
        <button class="pf-login" data-account="login">Войти</button>`;
      return;
    }
    box.innerHTML = `<button class="pf-user" data-account="menu" title="Аккаунт на сайте">
        <img src="${esc(user.avatar)}" alt="" onerror="this.style.visibility='hidden'">
        <span><b>${esc(user.login)}</b><small>${money(user.balance)}</small></span>
      </button>
      <div class="pf-menu" id="pf-menu" hidden>
        <button data-account="profile">Профиль на сайте</button>
        <button data-account="purse">Пополнить баланс</button>
        <button data-account="logout" class="danger">Выйти</button>
      </div>`;
  }

  // ---- sign-in dialog ----
  function openLogin() {
    closeLogin();
    const wrap = document.createElement('div');
    wrap.className = 'modal-wrap';
    wrap.id = 'login-modal';
    wrap.innerHTML = `<form class="modal login" id="login-form" autocomplete="off">
        <button type="button" class="modal-x" data-account="close" aria-label="Закрыть">×</button>
        <h2>Вход на epicentre-cs.ru</h2>
        <p class="muted">Аккаунт сайта: батлпасс, баланс, магазин и игра прямо в клиенте.</p>
        <label>Логин<input id="lg-login" maxlength="30" spellcheck="false" required></label>
        <label>Пароль<input id="lg-pass" type="password" maxlength="64" required></label>
        <p class="login-error" id="lg-error"></p>
        <button class="connect" id="lg-send" type="submit">Войти</button>
        <p class="muted small">Нет аккаунта? <a href="#" data-account="register">Регистрация на сайте</a></p>
      </form>`;
    document.body.appendChild(wrap);
    $('lg-login').focus();
    $('login-form').addEventListener('submit', async (e) => {
      e.preventDefault();
      if (preview) {
        user = { id: 1, login: $('lg-login').value || 'Игрок', avatar: 'logo.png', balance: 150 };
        closeLogin();
        emit();
        return;
      }
      $('lg-send').disabled = true;
      $('lg-error').textContent = '';
      const res = await signIn($('lg-login').value.trim(), $('lg-pass').value);
      $('lg-send').disabled = false;
      if (res.ok) closeLogin();
      else $('lg-error').textContent = res.message || 'Не удалось войти';
    });
  }

  function closeLogin() {
    const m = $('login-modal');
    if (m) m.remove();
  }

  document.addEventListener('click', (e) => {
    const menu = $('pf-menu');
    const t = e.target.closest('[data-account]');
    if (menu && !(t && t.dataset.account === 'menu') && !e.target.closest('#pf-menu')) menu.hidden = true;
    if (!t) {
      if (e.target.id === 'login-modal') closeLogin();
      return;
    }
    e.preventDefault();
    switch (t.dataset.account) {
      case 'login': return openLogin();
      case 'close': return closeLogin();
      case 'menu': if (menu) menu.hidden = !menu.hidden; return;
      case 'profile': return openSite('/profile?id=' + (user ? user.id : 0));
      case 'purse': return openSite('/purse');
      case 'logout': return signOut();
      case 'register': return openSite('/'); // the site's sign-up form is on its pages, not at its own URL
    }
  });
  document.addEventListener('keydown', (e) => { if (e.key === 'Escape') closeLogin(); });

  window.EpicAccount = {
    SITE,
    get user() { return user; },
    get signedIn() { return !!user; },
    preview,
    call, signIn, signOut, refresh, openSite, openLogin,
    onChange(fn) { listeners.push(fn); },
  };

  // ---- start: the saved token, checked against the site ----
  (async () => {
    if (!preview) user = cache.get();
    renderProfile();
    if (preview) return;
    try {
      token = await nc.account.getToken();
    } catch {
      token = null;
    }
    if (!token) {
      if (user) {
        user = null;
        emit();
      }
      return;
    }
    if (user) emit(); // signed in from the cache: pages may start loading right away
    const res = await call('me');
    if (res.ok) {
      user = res.user;
      emit();
    } else if (res.error !== 'auth' && !user) {
      emit(); // offline: keep the token for later
    }
  })();
})();
