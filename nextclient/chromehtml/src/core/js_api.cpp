#include "core/js_api.h"

#include <cstdio>

namespace nc {

namespace {

int hex(char c) {
    if (c >= '0' && c <= '9') return c - '0';
    if (c >= 'a' && c <= 'f') return c - 'a' + 10;
    if (c >= 'A' && c <= 'F') return c - 'A' + 10;
    return -1;
}

std::string percent_decode(std::string_view s) {
    std::string out;
    out.reserve(s.size());
    for (size_t i = 0; i < s.size(); ++i) {
        if (s[i] == '%' && i + 2 < s.size() && hex(s[i + 1]) >= 0 && hex(s[i + 2]) >= 0) {
            out += static_cast<char>(hex(s[i + 1]) * 16 + hex(s[i + 2]));
            i += 2;
        } else if (s[i] == '+') {
            out += ' ';
        } else {
            out += s[i];
        }
    }
    return out;
}

// SteamID account type 1 (individual) — CSteamID::BIndividualAccount in NextClient's code.
bool individual_account(uint64_t id) { return ((id >> 52) & 0xF) == 1; }

constexpr char kApiScript[] = R"JS((function () {
  if (window.nextclient && window.nextclient.__ncb) return;
  function url(method, arg) {
    return 'ncb://api/' + method + (arg === undefined ? '' : '?a=' + encodeURIComponent(String(arg)));
  }
  function call(method, arg) {
    try {
      var x = new XMLHttpRequest();
      x.open('GET', url(method, arg), false);
      x.send();
      return x.status === 200 ? JSON.parse(x.responseText) : null;
    } catch (e) {
      return null;
    }
  }
  function callAsync(method, arg) {
    return fetch(url(method, arg)).then(function (r) {
      if (!r.ok) throw new Error('nextclient.' + method + ': ' + r.status);
      return r.json();
    });
  }
  window.makeAsEventTarget = function (object) {
    var target = new EventTarget();
    Object.defineProperty(object, 'eventTarget', { get: function () { return target; } });
    object.addEventListener = function () { return target.addEventListener.apply(target, arguments); };
    object.removeEventListener = function () { return target.removeEventListener.apply(target, arguments); };
    object.dispatchEvent = function (type, params) {
      return target.dispatchEvent(new CustomEvent(type, { detail: params, cancelable: true }));
    };
  };
  // a page box (CSS pixels) as "x y w h" in device pixels
  function rectArg(rect) {
    var k = window.devicePixelRatio || 1;
    return [rect.left, rect.top, rect.width, rect.height].map(function (v) { return Math.round(v * k); }).join(' ');
  }
  var nextclient = window.nextclient = { __ncb: true };
  nextclient.exec = function (cmd) { if (typeof cmd == 'string') return call('exec', cmd); };
  nextclient.getCvarValue = function (name) { return call('getCvarValue', name); };
  Object.defineProperty(nextclient, 'connectedHost', { get: function () { return call('connectedHost'); } });
  makeAsEventTarget(nextclient);

  nextclient.gameUi = { optionsDialog: {} };
  Object.defineProperty(nextclient.gameUi, 'shown', { get: function () { return call('gameUiShown') === true; } });
  nextclient.gameUi.optionsDialog.open = function (tab) { if (typeof tab == 'string') call('openOptionsDialog', tab); };
  // NewGame, Options, Players, ServerBrowser, Resume, Disconnect, Quit, QuitNoConfirm (trusted pages)
  nextclient.gameUi.command = function (name) { return callAsync('gameUiCommand', String(name)); };
  // opens an allowed https link (vk.ru, epicentre-cs.ru) in the system browser (trusted pages)
  nextclient.openExternal = function (url) { return callAsync('openExternal', String(url)); };
  // "New game" (trusted pages): installed maps and a listen server start.
  // options: {map ('*' random), maxplayers, hostname, sv_password, <mp_*/bot_* cvar>: value}
  nextclient.game = {
    getMaps: function () { return callAsync('getMaps'); },
    start: function (options) {
      var lines = [];
      for (var k in options) if (options[k] !== undefined && options[k] !== null) lines.push(k + '\t' + String(options[k]));
      return callAsync('startGame', lines.join('\n'));
    }
  };
  // the site account (built-in menu only): the client API token, stored encrypted by the game
  nextclient.account = {
    getToken: function () { return callAsync('getAccountToken'); },
    setToken: function (token) { return callAsync('setAccountToken', token ? String(token) : ''); },
    // POST to the site's client API, sent by the game with the saved token: {status, body, error}
    request: function (form) { return callAsync('siteApi', String(form)); },
    // the native battle pass game (game token from api/client game_token), embedded over rect:
    // a DOMRect-like box in CSS pixels of this page
    launchGame: function (token, login, rect) {
      return callAsync('launchGame', String(token) + '\t' + encodeURIComponent(String(login || '')) + '\t' + rectArg(rect));
    },
    setGameRect: function (rect) { return callAsync('setGameRect', rectArg(rect)); },
    closeGame: function () { return callAsync('closeGame'); },
    gameRunning: function () { return callAsync('gameRunning'); },
    // input for the game ("k down vk repeat" / "m message x y flags"): it never takes the focus
    gameInput: function (line) { return callAsync('gameInput', String(line)); }
  };
  // the menu's own options screen (trusted pages): video modes and key bindings
  nextclient.settings = {
    getVideoState: function () { return callAsync('getVideoState'); },
    applyVideo: function (w, h, windowed, low) { return callAsync('applyVideo', [w | 0, h | 0, windowed ? 1 : 0, low ? 1 : 0].join(' ')); },
    getKeyBindings: function () { return callAsync('getKeyBindings'); },
    bindKey: function (key, binding) { return callAsync('bindKey', String(key) + '\t' + (binding ? String(binding) : '')); },
    // weapon preview over the page; rect in CSS pixels of this page, null hides it.
    // move: 0 run, 1 walk, 2 strafe, 3 idle; demo: 0 none, 1 lag, 2 weapon switch
    setGamePreview: function (rect, move, demo) {
      var k = window.devicePixelRatio || 1;
      var r = rect ? [rect.left, rect.top, rect.width, rect.height].map(function (v) { return Math.round(v * k); }) : [0, 0, 0, 0];
      return callAsync('setGamePreview', r.concat([move | 0, demo | 0]).join(' '));
    }
  };
  makeAsEventTarget(nextclient.gameUi);

  nextclient.console = {};
  makeAsEventTarget(nextclient.console);

  nextclient.matchmaking = {
    getServerInfo: function (ip) { return callAsync('getServerInfo', ip); },
    getPlayersInfo: function (ip) { return callAsync('getPlayersInfo', ip); },
    getRules: function (ip) { return callAsync('getRules', ip); },
    getFavoriteServers: function () { return callAsync('getFavoriteServers'); },
    getInternetServers: function () { return callAsync('getInternetServers'); },
    getHistoryServers: function () { return callAsync('getHistoryServers'); },
    addFavoriteServer: function (ip) { return callAsync('addFavoriteServer', ip); },
    removeFavoriteServer: function (ip) { return callAsync('removeFavoriteServer', ip); }
  };
  makeAsEventTarget(nextclient.matchmaking);
})();)JS";

// Floating "back to menu" button for sites opened from the main menu (the site forbids framing,
// so it replaces the menu page). Top-centre, clear of the game's own menu buttons bottom-left.
constexpr char kBackButtonScript[] = R"JS((function () {
  if (window.__ncbBack || window.top !== window) return;
  window.__ncbBack = true;
  function add() {
    if (!document.body || document.getElementById('__ncb-back')) return;
    var b = document.createElement('button');
    b.id = '__ncb-back';
    b.textContent = '← В меню';
    b.style.cssText = 'position:fixed;z-index:2147483647;top:10px;left:50%;transform:translateX(-50%);' +
      'padding:9px 18px;border:0;border-radius:10px;cursor:pointer;color:#fff;' +
      'font:700 13px/1 "Segoe UI",sans-serif;letter-spacing:.04em;' +
      'background:linear-gradient(135deg,#ff7a1a,#ff4d2e);box-shadow:0 6px 20px rgba(0,0,0,.45)';
    b.onclick = function () { location.href = 'ncl://menu/'; };
    document.body.appendChild(b);
  }
  if (document.readyState === 'loading') document.addEventListener('DOMContentLoaded', add);
  else add();
})();)JS";

// Shrinks a page wider than the panel (fixed-width MOTD tables) with CSS zoom so it fits without
// horizontal scrolling. Re-measured at natural size on every resize: panels start tiny and grow.
constexpr char kFitWidthScript[] = R"JS((function () {
  if (window.__ncbFit) return window.__ncbFit();
  var d = document.documentElement, timer = 0;
  function fit() {
    if (!document.body) return;
    d.style.zoom = '';
    var w = window.innerWidth, sw = Math.max(d.scrollWidth, document.body.scrollWidth);
    if (w > 200 && sw > w + 1) d.style.zoom = (w / sw).toFixed(4);
  }
  window.__ncbFit = fit;
  window.addEventListener('resize', function () { clearTimeout(timer); timer = setTimeout(fit, 50); });
  fit();
  setTimeout(fit, 600);
})();)JS";

}  // namespace

std::optional<ApiCall> parse_api_url(std::string_view url) {
    constexpr std::string_view prefix = "ncb://api/";
    if (url.substr(0, prefix.size()) != prefix) return std::nullopt;
    url.remove_prefix(prefix.size());
    size_t q = url.find('?');
    ApiCall call{std::string(url.substr(0, q)), ""};
    if (call.method.empty()) return std::nullopt;
    if (q != std::string_view::npos) {
        std::string_view query = url.substr(q + 1);
        if (query.substr(0, 2) == "a=") call.arg = percent_decode(query.substr(2));
    }
    return call;
}

std::string json_string(std::string_view s) {
    std::string out = "\"";
    for (unsigned char c : s) {
        switch (c) {
            case '"': out += "\\\""; break;
            case '\\': out += "\\\\"; break;
            case '\n': out += "\\n"; break;
            case '\r': out += "\\r"; break;
            case '\t': out += "\\t"; break;
            case '<': out += "\\u003c"; break;
            case '>': out += "\\u003e"; break;
            default:
                if (c < 0x20) {
                    char buf[8];
                    std::snprintf(buf, sizeof(buf), "\\u%04x", c);
                    out += buf;
                } else {
                    out += static_cast<char>(c);
                }
        }
    }
    return out + "\"";
}

std::string host_json(const std::optional<ConnectedHost>& host) {
    if (!host) return "null";
    std::string out = "{\"address\":" + json_string(host->address) + ",\"state\":" +
                      (host->joined ? "\"joined\"" : "\"loading\"") + ",\"players\":[";
    for (size_t i = 0; i < host->players.size(); ++i) {
        const PlayerInfo& p = host->players[i];
        if (i) out += ",";
        out += "{\"name\":" + json_string(p.name) + ",\"steamId\":\"" + std::to_string(p.steam_id) +
               "\",\"isInSteamMode\":" + (individual_account(p.steam_id) ? "true" : "false") +
               ",\"isLocalPlayer\":" + (p.local ? "true" : "false") + "}";
    }
    return out + "]}";
}

std::string dispatch_script(std::string_view target, std::string_view type, std::string_view detail_json) {
    // Guard every level: "nextclient.gameUi" -> window.nextclient&&window.nextclient.gameUi&&...
    std::string guard, path = "window";
    std::string_view rest = target;
    while (!rest.empty()) {
        size_t dot = rest.find('.');
        path += "." + std::string(rest.substr(0, dot));
        guard += path + "&&";
        rest = dot == std::string_view::npos ? std::string_view{} : rest.substr(dot + 1);
    }
    return guard + path + ".dispatchEvent(" + json_string(type) + "," + std::string(detail_json) + ");";
}

const char* api_script() { return kApiScript; }

bool is_main_menu_agent(std::string_view user_agent) { return user_agent.find("Main Menu") != std::string_view::npos; }

bool wants_back_button(std::string_view url) {
    return url.substr(0, 7) == "http://" || url.substr(0, 8) == "https://";
}

const char* back_button_script() { return kBackButtonScript; }

const char* fit_width_script() { return kFitWidthScript; }

bool api_resource_type_allowed(cef_resource_type_t type) { return type == RT_XHR; }  // fetch() reports RT_XHR too

}  // namespace nc
