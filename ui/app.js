'use strict';

/* ==========================================================================
   ToolBox — interface
   Le C++ envoie l'état ({type:"state"}) et les mesures en direct ({type:"live"}).
   L'interface répond avec des messages comme {type:"setModule", id, enabled}.
   ========================================================================== */

// ------------------------------------------------------------------ Outils

const $ = (sel, root = document) => root.querySelector(sel);
const $$ = (sel, root = document) => [...root.querySelectorAll(sel)];

function esc(s) {
  return String(s ?? '').replace(/[&<>"']/g, c => ({ '&': '&amp;', '<': '&lt;', '>': '&gt;', '"': '&quot;', "'": '&#39;' }[c]));
}

const nf1 = new Intl.NumberFormat('fr-CA', { maximumFractionDigits: 1 });
const nf0 = new Intl.NumberFormat('fr-CA', { maximumFractionDigits: 0 });

function fmtBytes(b) {
  if (b == null) return '—';
  const u = ['o', 'Ko', 'Mo', 'Go', 'To'];
  let i = 0;
  while (b >= 1024 && i < u.length - 1) { b /= 1024; i++; }
  return `${i >= 3 ? nf1.format(b) : nf0.format(b)} ${u[i]}`;
}
function fmtSpeed(bps) {
  if (bps == null) return '—';
  if (bps < 1024 * 1024) return `${nf0.format(bps / 1024)} Ko/s`;
  return `${nf1.format(bps / 1024 / 1024)} Mo/s`;
}
function fmtDuration(sec) {
  sec = Math.max(0, Math.floor(sec));
  const d = Math.floor(sec / 86400), h = Math.floor(sec % 86400 / 3600), m = Math.floor(sec % 3600 / 60);
  if (d) return `${d} j ${h} h`;
  if (h) return `${h} h ${m} min`;
  return `${m} min`;
}
function fmtAgo(ts) {
  if (!ts) return 'jamais';
  const s = Math.max(0, Math.round(Date.now() / 1000 - ts));
  if (s < 60) return `il y a ${s} s`;
  if (s < 3600) return `il y a ${Math.round(s / 60)} min`;
  if (s < 86400) return `il y a ${Math.round(s / 3600)} h`;
  return new Date(ts * 1000).toLocaleDateString('fr-CA', { day: 'numeric', month: 'long' });
}
function fileName(path) { return String(path || '').split(/[\\/]/).pop(); }

let toastTimer;
function toast(msg) {
  const t = $('#toast');
  t.textContent = msg;
  t.hidden = false;
  t.classList.remove('fade-in'); void t.offsetWidth; t.classList.add('fade-in');
  clearTimeout(toastTimer);
  toastTimer = setTimeout(() => { t.hidden = true; }, 1800);
}

// Icônes (traits 24×24, style Fluent)
const ICONS = {
  home: 'M4 10.5 12 4l8 6.5V19a1 1 0 0 1-1 1h-4.5v-5.5h-5V20H5a1 1 0 0 1-1-1z',
  monitor: 'M4 19V11M9.3 19V5M14.6 19v-7M20 19V8',
  keyboard: 'M3.5 6.5h17a1 1 0 0 1 1 1v9a1 1 0 0 1-1 1h-17a1 1 0 0 1-1-1v-9a1 1 0 0 1 1-1zM7 10h.01M10.5 10h.01M14 10h.01M17.5 10h.01M8 14h8',
  clipboard: 'M9 4.5h6M9 4.5a1 1 0 0 0-1 1V6a1 1 0 0 0 1 1h6a1 1 0 0 0 1-1v-.5a1 1 0 0 0-1-1M9 4.5H7a2 2 0 0 0-2 2V19a2 2 0 0 0 2 2h10a2 2 0 0 0 2-2V6.5a2 2 0 0 0-2-2h-2M9 12h6M9 16h4',
  convert: 'M4 8h13l-3.5-3.5M20 16H7l3.5 3.5',
  apps: 'M4.5 4.5h5v5h-5zM14.5 4.5h5v5h-5zM4.5 14.5h5v5h-5zM14.5 14.5h5v5h-5z',
  place: 'M12 21s-6.5-5.6-6.5-11a6.5 6.5 0 0 1 13 0C18.5 15.4 12 21 12 21zM12 12.5a2.5 2.5 0 1 0 0-5 2.5 2.5 0 0 0 0 5z',
  settings: 'M12 15a3 3 0 1 0 0-6 3 3 0 0 0 0 6zM19.4 13.5l1.6 1.2-2 3.4-1.9-.8a7 7 0 0 1-1.7 1l-.3 2h-4l-.3-2a7 7 0 0 1-1.7-1l-1.9.8-2-3.4 1.6-1.2a7 7 0 0 1 0-2L3 10.3l2-3.4 1.9.8a7 7 0 0 1 1.7-1l.3-2h4l.3 2a7 7 0 0 1 1.7 1l1.9-.8 2 3.4-1.6 1.2a7 7 0 0 1 0 2z',
  update: 'M20 12a8 8 0 1 1-2.4-5.7M20 4v4.5h-4.5',
  copy: 'M9 9h10v11H9zM5 15V4h10',
  pin: 'M9 4h6l-1 5 3 3v1H7v-1l3-3zM12 13v7',
  trash: 'M5 7h14M10 7V5h4v2M7 7l1 13h8l1-13',
  edit: 'M4 20h4L19 9l-4-4L4 16zM13.5 6.5l4 4',
  play: 'M8 5.5v13l10-6.5z',
  plus: 'M12 5v14M5 12h14',
  swap: 'M7 4 3 8l4 4M3 8h14M17 20l4-4-4-4M21 16H7',
  wifi: 'M2.5 9a14 14 0 0 1 19 0M5.5 12.5a9.5 9.5 0 0 1 13 0M8.5 16a5 5 0 0 1 7 0M12 19.5h.01',
  gps: 'M12 19a7 7 0 1 0 0-14 7 7 0 0 0 0 14zM12 2v3M12 19v3M2 12h3M19 12h3M12 14.5a2.5 2.5 0 1 0 0-5 2.5 2.5 0 0 0 0 5z',
  folder: 'M3.5 6.5a1 1 0 0 1 1-1h5l2 2h8a1 1 0 0 1 1 1V18a1 1 0 0 1-1 1h-15a1 1 0 0 1-1-1z',
  file: 'M6 3h8l4 4v14H6zM14 3v4h4',
  link: 'M10 14a4 4 0 0 0 5.7 0l3-3a4 4 0 0 0-5.7-5.7l-1 1M14 10a4 4 0 0 0-5.7 0l-3 3a4 4 0 0 0 5.7 5.7l1-1',
  power: 'M12 3v8M7.5 6.3a7 7 0 1 0 9 0',
  search: 'M10.5 17a6.5 6.5 0 1 0 0-13 6.5 6.5 0 0 0 0 13zM20 20l-4.8-4.8',
  close: 'M6 6l12 12M18 6 6 18',
  info: 'M12 21a9 9 0 1 0 0-18 9 9 0 0 0 0 18zM12 11v5M12 8h.01',
  startup: 'M5 12h11M12 7l5 5-5 5M20 4v16',
  cpu: 'M7 7h10v10H7zM10 10h4v4h-4zM10 3v4M14 3v4M10 17v4M14 17v4M3 10h4M3 14h4M17 10h4M17 14h4',
  star: 'M12 3.5l2.6 5.3 5.9.9-4.3 4.1 1 5.8L12 16.9l-5.2 2.7 1-5.8-4.3-4.1 5.9-.9z',
  stop: 'M7 7h10v10H7z',
  leaf: 'M5 19c0-8 5-13 14-14 0 9-5 14-13 14zM5 19l7-7',
  camera: 'M4 8.5a1.5 1.5 0 0 1 1.5-1.5h2.2l1.3-2h6l1.3 2h2.2A1.5 1.5 0 0 1 20 8.5v9a1.5 1.5 0 0 1-1.5 1.5h-13A1.5 1.5 0 0 1 4 17.5zM12 16a3.2 3.2 0 1 0 0-6.4 3.2 3.2 0 0 0 0 6.4z',
  chart: 'M4 4v16h16M8 16v-4M12 16V8M16 16v-6',
  drop: 'M12 3.5s6 6.4 6 10.5a6 6 0 0 1-12 0c0-4.1 6-10.5 6-10.5z',
  scan: 'M4 8V5a1 1 0 0 1 1-1h3M16 4h3a1 1 0 0 1 1 1v3M20 16v3a1 1 0 0 1-1 1h-3M8 20H5a1 1 0 0 1-1-1v-3M8 9.5h8M8 12.5h8M8 15.5h5',
  tray: 'M4 14h4l1.5 2.5h5L16 14h4M4 14l2.5-8h11l2.5 8v5H4z',
};
const icon = (name, cls = 'icon') => `<svg class="${cls}" viewBox="0 0 24 24"><path d="${ICONS[name] || ''}"/></svg>`;

// ------------------------------------------------------------------ Pont avec le C++

const webview = window.chrome && window.chrome.webview;
const S = { app: null, modules: {}, order: [], update: null };   // état reçu du C++
let live = {};                                                   // dernières mesures
const hist = { cpu: [], ram: [] };                            // 90 dernières secondes
const pendingPicks = new Map();

function send(msg) {
  if (webview) webview.postMessage(msg);
  else Mock.handle(msg);
}
const moduleAction = (id, action, payload = {}) => send({ type: 'moduleAction', id, action, payload });
const modState = id => (S.modules[id] && S.modules[id].state) || {};
const modOn = id => !!(S.modules[id] && S.modules[id].enabled);

function pickFile() {
  const requestId = String(Math.random());
  send({ type: 'pickFile', requestId });
  return new Promise(resolve => pendingPicks.set(requestId, resolve));
}

function receive(msg) {
  if (!msg || typeof msg !== 'object') return;
  switch (msg.type) {
    case 'state': {
      S.app = msg.app;
      S.update = msg.update;
      S.order = msg.modules.map(m => m.id);
      S.modules = Object.fromEntries(msg.modules.map(m => [m.id, m]));
      onState();
      break;
    }
    case 'live': {
      live = msg.live || {};
      if (live.processes) procData = live.processes;
      const mon = live.monitor;
      if (mon) {
        push(hist.cpu, mon.cpu);
        push(hist.ram, mon.ram ? 100 * mon.ram.used / mon.ram.total : null);
      }
      if (current.live) current.live();
      break;
    }
    case 'update':
      S.update = msg.update;
      if (currentId === 'updates' || currentId === 'home') renderPage(true);
      renderNav();
      break;
    case 'popupShown':
      Popup.shown();
      break;
    case 'filePicked': {
      const resolve = pendingPicks.get(msg.requestId);
      pendingPicks.delete(msg.requestId);
      if (resolve) resolve(msg.path || '');
      break;
    }
  }
}
function push(arr, v) { arr.push(v); if (arr.length > 90) arr.shift(); }

// ------------------------------------------------------------------ Navigation

const PAGES = [
  { id: 'home', label: 'Accueil', icon: 'home', keys: 'accueil resume', page: () => HomePage },
  // Toujours actives (pas d'interrupteur)
  { id: 'monitor', label: 'Moniteur', icon: 'monitor', module: 'monitor', keys: 'cpu processeur ram memoire disque batterie reseau internet vitesse', page: () => MonitorPage },
  { id: 'converter', label: 'Convertisseur', icon: 'convert', module: 'converter', keys: 'unites devises argent dollar euro temperature longueur masse', page: () => ConverterPage },
  { sep: true },
  { id: 'stats', label: 'Statistiques', icon: 'chart', module: 'stats', keys: 'statistiques stats temps ecran utilisation historique chiffres', page: () => StatsPage },
  { id: 'processes', label: 'Programmes', icon: 'cpu', module: 'processes', keys: 'taches tuer fermer arreter relancer ressources lent mode leger', page: () => ProcessesPage },
  { id: 'enter_guard', label: 'Garde Enter', icon: 'keyboard', module: 'enter_guard', keys: 'a clavier touche accident entree faute', page: () => EnterGuardPage },
  { id: 'clipboard', label: 'Presse-papiers', icon: 'clipboard', module: 'clipboard', keys: 'copier coller ctrl c v historique texte', page: () => ClipboardPage },
  { id: 'ocr', label: "Capture d'écran", icon: 'camera', module: 'ocr', keys: 'capture screenshot image png ocr tesseract texte ecran lire copier scanner photo', page: () => OcrPage },
  { id: 'color', label: 'Pipette', icon: 'drop', module: 'color', keys: 'pipette couleur color picker hex rgb hsl pixel', page: () => ColorPage },
  { id: 'app_launcher', label: "Raccourcis d'apps", icon: 'apps', module: 'app_launcher', keys: 'lancer groupe raccourci ouvrir apps', page: () => AppLauncherPage },
  { id: 'place_launcher', label: 'Lancement par lieu', icon: 'place', module: 'place_launcher', keys: 'gps wifi maison ecole lieu adresse position', page: () => PlacePage },
  { sep: true },
  { id: 'settings', label: 'Paramètres', icon: 'settings', keys: 'demarrage windows options reglages', page: () => SettingsPage },
  { id: 'updates', label: 'Mises à jour', icon: 'update', keys: 'version nouveautes maj', page: () => UpdatesPage },
  { url: 'https://mede2026.github.io/pnyx-privacy/apps/index.html', label: 'Mes apps', icon: 'link', keys: 'site web internet pnyx' },
];

let currentId = (() => { try { return localStorage.getItem('page') || 'home'; } catch { return 'home'; } })();
let current = {};

function go(id) {
  if (hotkeyCapturing) stopHotkeyCapture(true);
  currentId = id;
  try { localStorage.setItem('page', id); } catch { /* ignore */ }
  renderNav();
  renderPage(true);
  $('#main').scrollTop = 0;
}

let navQuery = '';
const fold = t => String(t || '').toLowerCase().normalize('NFD').replace(/[\u0300-\u036f]/g, '');

function navMatches(p) {
  if (!navQuery) return true;
  const hay = fold([p.label, p.keys, p.module && S.modules[p.module]?.description].join(' '));
  return fold(navQuery).split(/\s+/).filter(Boolean).every(w => hay.includes(w));
}

function renderNav() {
  const nav = $('#nav');
  const items = PAGES.filter(p => p.sep ? !navQuery : navMatches(p));
  nav.innerHTML = items.map(p => {
    if (p.sep) return '<div class="nav-sep"></div>';
    if (p.url) return `<div class="nav-item" role="button" tabindex="0" data-url="${esc(p.url)}" title="${esc(p.url)}">${icon(p.icon)}<span>${esc(p.label)}</span><span style="margin-left:auto;color:var(--text-3)">↗</span></div>`;
    const m = p.module && S.modules[p.module];
    const off = m && !m.enabled;
    const badge = p.id === 'updates' && S.update && ['available', 'ready'].includes(S.update.status);
    const right = m && !m.alwaysOn
      ? `<input type="checkbox" class="switch sm" data-module-switch="${p.module}" ${m.enabled ? 'checked' : ''} title="${m.enabled ? 'Désactiver' : 'Activer'} ${esc(p.label)}">`
      : badge ? '<span class="dot" style="background:var(--accent);opacity:1"></span>' : '';
    return `<div class="nav-item${p.id === currentId ? ' active' : ''}${off ? ' off' : ''}" role="button" tabindex="0" data-go="${p.id}">
      ${icon(p.icon)}<span>${esc(p.label)}</span>${right}
    </div>`;
  }).join('') || '<div class="nav-empty">Aucune fonction trouvée</div>';
}

$('#nav-search').addEventListener('input', e => { navQuery = e.target.value.trim(); renderNav(); });
$('#nav-search').addEventListener('keydown', e => {
  if (e.key === 'Enter') {
    const first = $('#nav [data-go], #nav [data-url]');
    if (first) first.click();
  } else if (e.key === 'Escape') { e.target.value = ''; navQuery = ''; renderNav(); }
});
document.addEventListener('keydown', e => {
  if (e.ctrlKey && e.key.toLowerCase() === 'f') { e.preventDefault(); $('#nav-search').focus(); $('#nav-search').select(); }
  if ((e.key === 'Enter' || e.key === ' ') && e.target.classList?.contains('nav-item')) { e.preventDefault(); e.target.click(); }
});

function onState() {
  if (window.TOOLBOX_POPUP) { Popup.render(); return; }
  $('#brand-version').textContent = S.app ? S.app.version : '';
  const g = $('#global-switch');
  g.checked = !!(S.app && S.app.enabled);
  $('#global-label').textContent = g.checked ? 'Activé' : 'En pause';
  renderNav();
  renderPage(false);
}

// Re-rendu : complet au changement de page, sinon on évite d'écraser un champ en cours d'édition.
function renderPage(force) {
  if (!S.app || window.TOOLBOX_POPUP) return;
  const def = PAGES.find(p => p.id === currentId) || PAGES[0];
  const Page = def.page();
  const page = $('#page');
  const editing = page.contains(document.activeElement) &&
    ['INPUT', 'TEXTAREA', 'SELECT'].includes(document.activeElement.tagName) &&
    document.activeElement.type !== 'checkbox';

  if (!force && current === Page && Page.update) { Page.update(); return; }
  if (!force && current === Page && (editing || $('#dialog').open)) return;

  current = Page;
  if (def.module && !modOn(def.module)) {
    page.innerHTML = moduleHead(def) + `
      <div class="card empty fade-in">
        ${icon(def.icon)}
        <h2>Cette fonction est désactivée</h2>
        <p class="muted">${esc(S.modules[def.module]?.description || '')}</p>
        <button class="btn primary" data-toggle-module="${def.module}" data-value="1">Activer</button>
      </div>`;
    current = {};
    return;
  }
  page.innerHTML = (def.module ? moduleHead(def) : def.id === 'home' ? '' : `<h1>${esc(def.label)}</h1>`) + Page.render();
  page.firstElementChild?.nextElementSibling?.classList.add('fade-in');
  Page.bind?.(page);
  Page.live?.();
}

function moduleHead(def) {
  const m = S.modules[def.module];
  if (m && m.alwaysOn) return `<div class="page-head"><h1>${esc(def.label)}</h1></div>
    <p class="page-desc">${esc(m.description)}</p>`;
  return `<div class="page-head"><h1>${esc(def.label)}</h1><span class="spacer"></span>
    <label class="row" title="Activer / désactiver cette fonction">
      <span class="muted">${m && m.enabled ? 'Activé' : 'Désactivé'}</span>
      <input type="checkbox" class="switch" data-module-switch="${def.module}" ${m && m.enabled ? 'checked' : ''}>
    </label></div>
    <p class="page-desc">${esc(m ? m.description : '')}</p>`;
}

// Délégation d'événements communs
document.addEventListener('click', e => {
  if (e.target.matches('input[type=checkbox]')) return;  // un interrupteur ne change pas de page
  const t = e.target.closest('[data-go],[data-toggle-module],[data-url]');
  if (!t) return;
  if (t.dataset.url) window.open(t.dataset.url, '_blank');  // le C++ l'ouvre dans le navigateur
  if (t.dataset.go) go(t.dataset.go);
  if (t.dataset.toggleModule) send({ type: 'setModule', id: t.dataset.toggleModule, enabled: t.dataset.value === '1' });
});
document.addEventListener('change', e => {
  const t = e.target;
  if (t.dataset.moduleSwitch) send({ type: 'setModule', id: t.dataset.moduleSwitch, enabled: t.checked });
});
$('#global-switch').addEventListener('change', e => send({ type: 'setGlobal', enabled: e.target.checked }));

function setRangeFill(r) {
  const p = (r.value - r.min) / (r.max - r.min) * 100;
  r.style.setProperty('--p', p + '%');
}

// ------------------------------------------------------------------ Jauge ronde

const G = { cx: 100, cy: 100, r: 84 };
const ARC_LEN = 0.75 * 2 * Math.PI * G.r;
function arcPath() {
  const a0 = 135 * Math.PI / 180, a1 = 45 * Math.PI / 180;
  const p = a => `${(G.cx + G.r * Math.cos(a)).toFixed(2)} ${(G.cy + G.r * Math.sin(a)).toFixed(2)}`;
  return `M ${p(a0)} A ${G.r} ${G.r} 0 1 1 ${p(a1)}`;
}
function gaugeHTML(id, caption, extra = '') {
  return `<div class="gauge ${extra}" id="g-${id}">
    <svg viewBox="0 0 200 184"><path class="track" d="${arcPath()}" stroke-width="15"/>
    <path class="bar" d="${arcPath()}" stroke-width="15" stroke-dasharray="0 1000"/></svg>
    <div class="center"><span class="num na">—</span></div>
    <div class="caption">${esc(caption)}</div></div>`;
}
function setGauge(id, pct, { text, unit = '%', hotAt = 80, critAt = 92 } = {}) {
  const g = document.getElementById('g-' + id);
  if (!g) return;
  const bar = $('.bar', g), num = $('.num', g);
  if (pct == null || isNaN(pct)) {
    bar.setAttribute('stroke-dasharray', '0 1000'); bar.style.opacity = 0;
    num.className = 'num na'; num.innerHTML = '—';
    return;
  }
  const p = Math.max(0, Math.min(100, pct));
  bar.style.opacity = p <= 0.3 ? 0 : 1;
  bar.setAttribute('stroke-dasharray', `${(ARC_LEN * p / 100).toFixed(1)} 1000`);
  g.classList.toggle('hot', hotAt != null && p >= hotAt && p < critAt);
  g.classList.toggle('crit', critAt != null && p >= critAt);
  num.className = 'num';
  num.innerHTML = `${esc(text ?? Math.round(p))}<span class="unit">${esc(unit)}</span>`;
}

// Petit graphique en ligne (historique)
function chartSVG(series, { max = 100, height = 180, points = 90 } = {}) {
  const W = 900, H = height, padL = 44, padB = 6, n = points;
  const x = i => padL + (W - padL) * i / (n - 1);
  const y = v => (H - padB) - (H - padB - 6) * Math.min(v, max) / max;
  let grid = '';
  for (const v of [0, 50, 100]) {
    grid += `<line class="grid-line" x1="${padL}" x2="${W}" y1="${y(v)}" y2="${y(v)}"/>
      <text class="axis" x="${padL - 8}" y="${y(v) + 4}" text-anchor="end">${v}%</text>`;
  }
  const lines = series.map(({ data, cls }) => {
    const pts = data.map((v, i) => v == null ? null : [x(i + n - data.length), y(v)]).filter(Boolean);
    if (pts.length < 2) return '';
    const d = pts.map((p, i) => `${i ? 'L' : 'M'}${p[0].toFixed(1)} ${p[1].toFixed(1)}`).join(' ');
    const area = cls ? '' : `<path class="area" d="${d} L${pts[pts.length - 1][0]} ${y(0)} L${pts[0][0]} ${y(0)} Z"/>`;
    return `${area}<path class="line ${cls || ''}" d="${d}"/>`;
  }).join('');
  return `<svg class="chart" viewBox="0 0 ${W} ${H}" preserveAspectRatio="none">
    <defs><linearGradient id="chartFill" x1="0" x2="0" y1="0" y2="1"><stop offset="0" stop-color="#7cbcff" stop-opacity=".28"/><stop offset="1" stop-color="#7cbcff" stop-opacity="0"/></linearGradient></defs>
    ${grid}${lines}</svg>`;
}

// ------------------------------------------------------------------ Accueil

let qcValue = '';

const clockText = d => `${d.getHours()}:${String(d.getMinutes()).padStart(2, '0')}`;

function greeting() {
  const h = new Date().getHours();
  return h < 5 ? 'Bonne nuit' : h < 12 ? 'Bon matin' : h < 18 ? 'Bon après-midi' : 'Bonsoir';
}

// Convertisseur rapide : « 10 km en mi », « 20 cad en usd », « 25 c en f »
const QC_ALIASES = { pouce: 'in', pouces: 'in', po: 'in', pied: 'ft', pieds: 'ft', pi: 'ft', verge: 'yd', mille: 'mi', milles: 'mi',
  livre: 'lb', livres: 'lb', lbs: 'lb', once: 'oz', gramme: 'g', grammes: 'g', kilo: 'kg', kilos: 'kg', litre: 'l', litres: 'l',
  '°c': 'c', '°f': 'f', celsius: 'c', fahrenheit: 'f', kelvin: 'k', 'km/h': 'kmh', 'm/s': 'ms', noeud: 'kn', noeuds: 'kn',
  gallon: 'gal', gallons: 'gal', tasse: 'cup', tasses: 'cup', heure: 'h', heures: 'h', minute: 'min', minutes: 'min',
  seconde: 's', secondes: 's', jour: 'd', jours: 'd', semaine: 'w', semaines: 'w', an: 'y', ans: 'y', annee: 'y', annees: 'y',
  '$': 'CAD', '€': 'EUR', m2: 'm2', 'm²': 'm2', 'pi²': 'ft2' };
function quickConvert(text) {
  const m = fold(text).trim().match(/^(-?[\d\s.,]+)\s*([^\s\d]\S*)\s+(?:en|to|in|vers|->|→|=)\s+(\S+)$/);
  if (!m) return null;
  const v = parseNum(m[1]);
  const norm = u => QC_ALIASES[u] || u;
  const from = norm(m[2]), to = norm(m[3]);
  for (const [cat, def] of Object.entries(UNITS)) {
    const keys = Object.keys(def.units);
    const find = u => keys.find(k => k.toLowerCase() === u.toLowerCase());
    const a = find(from), b = find(to);
    if (a && b && (cat !== 'money' || conv.rates)) {
      const r = convert(cat, a, b, v);
      if (isFinite(r)) return `${fmtNum(v, cat)} ${a} = <b>${fmtNum(r, cat)} ${b}</b>`;
    }
    if (a && b && cat === 'money') { loadRates(); return 'Chargement des taux…'; }
  }
  return null;
}

const HomePage = {
  render() {
    const on = S.app.enabled;
    const name = S.app.userName ? `, ${esc(S.app.userName)}` : '';
    const enabledCount = S.order.filter(id => modOn(id)).length;
    const now = new Date();
    return `
    <div class="home-hero">
      <div>
        <div class="home-date" id="h-date">${esc(now.toLocaleDateString('fr-CA', { weekday: 'long', day: 'numeric', month: 'long' }))}</div>
        <div class="home-hello">${greeting()}${name} 👋</div>
        <div class="row wrap" style="margin-top:14px">
          <span class="pill ${on ? 'ok' : 'warn'}"><span class="led"></span>${on ? `Actif · ${enabledCount} fonctions sur ${S.order.length}` : 'En pause'}</span>
          ${S.update && ['available', 'ready'].includes(S.update.status) ? `<span class="pill" data-go="updates" style="cursor:pointer">${icon('update')} Mise à jour ${esc(S.update.latest)}</span>` : ''}
        </div>
      </div>
      <div class="home-clock" id="h-clock">${clockText(now)}</div>
    </div>

    <div class="card home-gauges" data-go="monitor" title="Ouvrir le moniteur">
      ${gaugeHTML('h-cpu', 'Processeur', 'small')}${gaugeHTML('h-ram', 'Mémoire', 'small')}
      ${gaugeHTML('h-disk', 'Disque', 'small')}${gaugeHTML('h-bat', 'Batterie', 'small')}
    </div>

    <h2 class="home-section">Accès rapide</h2>
    <div class="grid home-grid">
      ${this.card('converter', 'convert', 'Convertir', `
        <input type="text" id="qc" placeholder="Ex. : 10 km en mi" value="${esc(qcValue)}">
        <div class="qc-out" id="qc-out">${quickConvert(qcValue) || '<span class="faint qc-hint">Aussi : 25 c en f · 20 cad en usd · 3 h en min</span>'}</div>`)}
      ${this.card('clipboard', 'clipboard', 'Presse-papiers', this.clipBody())}
      ${this.card('app_launcher', 'apps', "Raccourcis d'apps", this.appsBody())}
      ${this.card('processes', 'cpu', 'Programmes', this.procBody())}
      ${this.card('place_launcher', 'place', 'Lieux', this.placeBody())}
      ${this.card('ocr', 'camera', "Capture d'écran", this.ocrBody())}
    </div>

    <h2 class="home-section">Toutes les fonctions</h2>
    <div class="grid home-grid">${S.order.map(id => {
      const m = S.modules[id];
      const def = PAGES.find(p => p.module === id) || {};
      return `<div class="card tight home-tile${m.enabled ? '' : ' off'}" data-go="${def.id}">
        ${icon(def.icon || 'apps')}<div class="main"><b>${esc(m.name)}</b><div class="faint small ellipsis">${esc(m.description)}</div></div>
        ${m.alwaysOn ? '<span class="chip" title="Toujours active">Toujours</span>' : `<input type="checkbox" class="switch sm" data-module-switch="${id}" ${m.enabled ? 'checked' : ''}>`}
      </div>`;
    }).join('')}</div>`;
  },
  card(id, ic, title, body) {
    const def = PAGES.find(p => p.module === id) || {};
    if (!modOn(id)) {
      return `<div class="card home-card off"><div class="home-card-head">${icon(ic)}<h3>${esc(title)}</h3></div>
        <div class="faint">Fonction désactivée.</div>
        <div><button class="btn" data-toggle-module="${id}" data-value="1">Activer</button></div></div>`;
    }
    return `<div class="card home-card"><div class="home-card-head">${icon(ic)}<h3 title="${esc(title)}">${esc(title)}</h3>
      <button class="btn ghost icon-only open-btn" data-go="${def.id}" title="Ouvrir ${esc(title)}">→</button></div>${body}</div>`;
  },
  clipBody() {
    const s = modState('clipboard');
    const items = (s.items || []).slice(0, 3);
    return `${s.hotkeyEnabled && s.hotkeyLabel ? `<div class="faint small">Fenêtre rapide : <kbd class="small">${esc(s.hotkeyLabel)}</kbd></div>` : ''}
      <div class="list">${items.length ? items.map(i => `<div class="quick-item" data-clip-id="${i.id}" title="Cliquer pour copier">${esc(i.text.slice(0, 120))}</div>`).join('')
        : '<div class="faint">Rien de copié pour l\'instant.</div>'}</div>`;
  },
  appsBody() {
    const groups = (modState('app_launcher').groups || []).slice(0, 4);
    return groups.length ? `<div class="quick-apps">${groups.map(g => `<button class="btn" data-launch-group="${g.id}" ${g.items.length ? '' : 'disabled'}><span style="font-size:18px">${esc(g.emoji || '🚀')}</span> ${esc(g.name)}</button>`).join('')}</div>`
      : `<div class="faint">Aucun groupe.</div><div><button class="btn" data-go="app_launcher">${icon('plus')} Créer un groupe</button></div>`;
  },
  procBody() {
    const s = modState('processes');
    const n = (s.useless || []).length, r = (s.stopped || []).length;
    return `<div class="faint">${n} programme${n > 1 ? 's' : ''} marqué${n > 1 ? 's' : ''} inutile${n > 1 ? 's' : ''} · ${r} à relancer</div>
      <div class="row wrap"><button class="btn primary" data-home-pm="stopUseless" ${n ? '' : 'disabled'}>${icon('leaf')} Mode léger</button>
      <button class="btn" data-home-pm="relaunchAll" ${r ? '' : 'disabled'}>${icon('update')} Tout relancer</button></div>`;
  },
  placeBody() {
    const s = modState('place_launcher');
    const rules = s.rules || [];
    if (!rules.length) return `<div class="faint">Aucun lieu.</div><div><button class="btn" data-go="place_launcher">${icon('plus')} Ajouter un lieu</button></div>`;
    return `<div class="list">${rules.slice(0, 3).map(r => `<div class="row" style="gap:10px">
      <span class="led-dot ${r.inside ? 'ok' : ''}"></span><b class="ellipsis">${esc(r.place || r.ssid || 'Lieu')}</b>
      <span class="spacer"></span><span class="faint small">${r.inside ? 'Sur place' : 'Ailleurs'}</span></div>`).join('')}</div>
      ${(s.connected || []).length ? `<div class="faint small">${icon('wifi', 'icon xs')} ${esc(s.connected.join(', '))}</div>` : ''}`;
  },
  ocrBody() {
    const s = modState('ocr');
    const last = (s.history || [])[0];
    return `<div class="row wrap" style="gap:6px">
        <button class="btn primary" data-home-capture="shot" title="${esc(s.shot_hotkeyLabel || '')}">${icon('camera')} Image</button>
        <button class="btn" data-home-capture="text" title="${esc(s.hotkeyLabel || '')}">${icon('scan')} Texte</button></div>
      <div class="faint small">${s.shot_hotkeyEnabled ? `Image <kbd class="small">${esc(s.shot_hotkeyLabel)}</kbd>` : ''} ${s.hotkeyEnabled ? `Texte <kbd class="small">${esc(s.hotkeyLabel)}</kbd>` : ''}</div>
      ${last ? `<div class="quick-item" data-ocr-copy="${last.id}" title="Cliquer pour copier">${esc(last.text.slice(0, 120))}</div>` : ''}`;
  },
  guardBody() {
    const s = modState('enter_guard');
    return `<div class="row" style="gap:16px"><kbd>${esc(s.keyName || '?')}</kbd>
      <div><div class="value big">${nf0.format(s.corrections || 0)}</div><div class="faint small">frappe${s.corrections > 1 ? 's' : ''} accidentelle${s.corrections > 1 ? 's' : ''} corrigée${s.corrections > 1 ? 's' : ''}</div></div></div>`;
  },
  bind(root) {
    const qc = $('#qc', root);
    qc?.addEventListener('input', () => { qcValue = qc.value; $('#qc-out').innerHTML = quickConvert(qcValue) || '<span class="faint qc-hint">Ex. : 10 km en mi · 25 c en f</span>'; });
    root.addEventListener('click', e => {
      const clip = e.target.closest('[data-clip-id]');
      if (clip) { moduleAction('clipboard', 'copy', { id: +clip.dataset.clipId }); toast('Copié !'); }
      const g = e.target.closest('[data-launch-group]');
      if (g) { moduleAction('app_launcher', 'launchGroup', { id: g.dataset.launchGroup }); toast('Lancement…'); }
      const cap = e.target.closest('[data-home-capture]');
      if (cap) send({ type: 'ocrCapture', shot: cap.dataset.homeCapture === 'shot' });
      const oc = e.target.closest('[data-ocr-copy]');
      if (oc) { moduleAction('ocr', 'copy', { id: +oc.dataset.ocrCopy }); toast('Copié !'); }
      const pm = e.target.closest('[data-home-pm]');
      if (pm) { moduleAction('processes', pm.dataset.homePm); toast(pm.dataset.homePm === 'stopUseless' ? 'Mode léger activé' : 'Relance…'); }
    });
  },
  live() {
    const now = new Date();
    const clock = document.getElementById('h-clock');
    if (clock) clock.textContent = clockText(now);
    const m = live.monitor;
    if (!m) return;
    setGauge('h-cpu', m.cpu);
    if (m.ram) setGauge('h-ram', 100 * m.ram.used / m.ram.total);
    if (m.disk) setGauge('h-disk', 100 * m.disk.used / m.disk.total, { hotAt: 85, critAt: 95 });
    if (m.battery) setGauge('h-bat', m.battery.percent, { hotAt: null, critAt: null }); else setGauge('h-bat', null);
  },
};

// ------------------------------------------------------------------ Moniteur

const MonitorPage = {
  render() {
    return `
    <div class="grid grid-4">
      <div class="card gauge-card">${gaugeHTML('cpu', 'Processeur', 'small')}<div class="detail" id="d-cpu"></div></div>
      <div class="card gauge-card">${gaugeHTML('ram', 'Mémoire', 'small')}<div class="detail" id="d-ram"></div></div>
      <div class="card gauge-card">${gaugeHTML('disk', 'Disque', 'small')}<div class="detail" id="d-disk"></div></div>
      <div class="card gauge-card">${gaugeHTML('bat', 'Batterie', 'small')}<div class="detail" id="d-bat"></div></div>
    </div>
    <div class="card" style="margin-top:18px">
      <div class="row"><h2 style="margin:0">Dernières 90 secondes</h2><span class="spacer"></span>
        <div class="legend"><span><i style="background:var(--accent)"></i>Processeur</span><span><i style="background:#f2a93b"></i>Mémoire</span></div></div>
      <div id="chart" style="margin-top:14px"></div>
    </div>
    <div class="grid grid-3">
      <div class="card tight"><div class="label">Téléchargement</div><div class="value" id="m-down">—</div></div>
      <div class="card tight"><div class="label">Envoi</div><div class="value" id="m-up">—</div></div>
      <div class="card tight"><div class="label">Allumé depuis</div><div class="value" id="m-uptime">—</div></div>
    </div>`;
  },
  live() {
    const m = live.monitor;
    if (!m) return;
    const set = (id, v) => { const el = document.getElementById(id); if (el) el.textContent = v; };
    setGauge('cpu', m.cpu);
    set('d-cpu', `${m.cores} cœurs logiques`);
    if (m.ram) {
      setGauge('ram', 100 * m.ram.used / m.ram.total);
      set('d-ram', `${fmtBytes(m.ram.used)} / ${fmtBytes(m.ram.total)}`);
    }
    if (m.disk) {
      setGauge('disk', 100 * m.disk.used / m.disk.total, { hotAt: 85, critAt: 95 });
      set('d-disk', `${m.disk.drive} · ${fmtBytes(m.disk.total - m.disk.used)} libres`);
    }
    if (m.battery) {
      setGauge('bat', m.battery.percent, { hotAt: null, critAt: null });
      const g = document.getElementById('g-bat');
      if (g) g.classList.toggle('crit', !m.battery.charging && m.battery.percent <= 15);
      const left = m.battery.secondsLeft > 0 ? ` · ${fmtDuration(m.battery.secondsLeft)} restantes` : '';
      set('d-bat', (m.battery.charging ? 'En charge ⚡' : 'Sur batterie') + left + (m.battery.saver ? ' · économie' : ''));
    } else {
      setGauge('bat', null);
      set('d-bat', 'Pas de batterie (secteur)');
    }
    set('m-down', m.net ? fmtSpeed(m.net.down) : '—');
    set('m-up', m.net ? fmtSpeed(m.net.up) : '—');
    set('m-uptime', fmtDuration(m.uptime));
    const c = document.getElementById('chart');
    if (c) c.innerHTML = chartSVG([{ data: hist.cpu }, { data: hist.ram, cls: 'alt' }]);
  },
};

// ------------------------------------------------------------------ Programmes

let procData = null;
let procQuery = '';
let procSort = 'ram';
let procWatchTimer = null;

const ProcessesPage = {
  render() {
    return `
    <div id="pm-top"></div>
    <div class="row" style="margin:22px 0 12px">
      <div style="position:relative;flex:1">
        <input type="search" id="pm-search" placeholder="Rechercher un programme…" value="${esc(procQuery)}" style="padding-left:38px">
        <span style="position:absolute;left:11px;top:10px;color:var(--text-3)">${icon('search')}</span>
      </div>
      <select id="pm-sort" style="width:190px">
        <option value="ram" ${procSort === 'ram' ? 'selected' : ''}>Trier par mémoire</option>
        <option value="cpu" ${procSort === 'cpu' ? 'selected' : ''}>Trier par processeur</option>
        <option value="name" ${procSort === 'name' ? 'selected' : ''}>Trier par nom</option>
      </select>
    </div>
    <div class="list" id="pm-list"><div class="card empty">Chargement de la liste…</div></div>
    <div id="pm-stopped"></div>
    <p class="faint small" style="margin-top:18px">🔒 Les programmes de Windows sont cachés et protégés.<br><b>Arrêter</b> ferme proprement, comme le ✕ de la fenêtre (le programme peut demander d'enregistrer). <b>Forcer</b> l'arrête immédiatement : le travail non enregistré est perdu, à utiliser seulement si le programme ne répond plus.</p>`;
  },
  bind(root) {
    const watch = () => { if (current === ProcessesPage && modOn('processes')) moduleAction('processes', 'watch'); else { clearInterval(procWatchTimer); procWatchTimer = null; } };
    clearInterval(procWatchTimer);
    procWatchTimer = setInterval(watch, 3000);
    watch();
    $('#pm-search', root).addEventListener('input', e => { procQuery = e.target.value; this.renderList(); });
    $('#pm-sort', root).addEventListener('change', e => { procSort = e.target.value; this.renderList(); });
    root.addEventListener('click', e => {
      const b = e.target.closest('[data-pm]');
      if (!b) return;
      const act = b.dataset.pm, key = b.dataset.key;
      if (act === 'toggleUseless' && procData) {
        const p = procData.programs.find(x => x.key === key);
        if (p) { p.useless = !p.useless; this.renderList(); this.renderTop(); }
      }
      if (act === 'stopUseless') { moduleAction('processes', 'stopUseless'); toast('Mode léger activé'); }
      else if (act === 'relaunchAll') moduleAction('processes', 'relaunchAll');
      else moduleAction('processes', act, { key });
      if (act === 'stop' || act === 'forceStop') { b.disabled = true; b.textContent = '…'; }
    });
    this.update();
  },
  update() { this.renderTop(); this.renderList(); this.renderStopped(); },
  live() { this.renderTop(); this.renderList(); },
  renderTop() {
    const el = document.getElementById('pm-top');
    if (!el) return;
    const s = modState('processes');
    const progs = procData ? procData.programs : [];
    const uselessRunning = progs.filter(p => p.useless);
    const ram = uselessRunning.reduce((a, p) => a + p.ram, 0);
    const total = progs.reduce((a, p) => a + p.ram, 0);
    el.innerHTML = `
    ${s.lastEvent ? `<div class="banner info">${icon('info')}<div class="text">${esc(s.lastEvent)}</div></div>` : ''}
    <div class="card">
      <div class="row wrap" style="gap:40px">
        <div><div class="label">Programmes ouverts</div><div class="value big">${procData ? progs.length : '—'}</div></div>
        <div><div class="label">Mémoire utilisée par eux</div><div class="value big">${procData ? fmtBytes(total) : '—'}</div></div>
        <div><div class="label">Inutiles ouverts</div><div class="value big">${uselessRunning.length}${ram ? `<span class="muted" style="font-size:16px;font-weight:500"> · ${fmtBytes(ram)}</span>` : ''}</div></div>
        <span class="spacer"></span>
        <div class="row wrap">
          <button class="btn primary big" data-pm="stopUseless" ${uselessRunning.length ? '' : 'disabled'}>${icon('leaf')} Mode léger</button>
          <button class="btn big" data-pm="relaunchAll" ${(s.stopped || []).length ? '' : 'disabled'}>${icon('update')} Tout relancer</button>
        </div>
      </div>
      <div class="faint small" style="margin-top:14px">Clique sur ★ pour marquer un programme comme inutile. « Mode léger » les arrête tous d'un coup.</div>
    </div>`;
  },
  renderList() {
    const el = document.getElementById('pm-list');
    if (!el || !procData) return;
    const q = procQuery.trim().toLowerCase();
    let progs = procData.programs.filter(p => !q || (p.name + ' ' + p.exe + ' ' + p.title).toLowerCase().includes(q));
    progs.sort(procSort === 'name' ? (a, b) => a.name.localeCompare(b.name, 'fr') : procSort === 'cpu' ? (a, b) => b.cpu - a.cpu : (a, b) => b.ram - a.ram);
    if (!progs.length) { el.innerHTML = `<div class="card empty">${q ? 'Aucun résultat' : 'Aucun programme à afficher'}</div>`; return; }
    el.innerHTML = progs.map(p => `
      <div class="list-item pm-row">
        <button class="btn ghost icon-only" data-pm="toggleUseless" data-key="${esc(p.key)}" title="${p.useless ? 'Retirer des inutiles' : 'Marquer comme inutile'}" style="color:${p.useless ? 'var(--warn)' : 'var(--text-3)'}">
          <svg class="icon" viewBox="0 0 24 24" style="${p.useless ? 'fill:currentColor' : ''}"><path d="${ICONS.star}"/></svg></button>
        <div class="main">
          <div class="row" style="gap:8px;min-width:0"><b class="ellipsis" title="${esc(p.name)}">${esc(p.name)}</b><span class="pm-chips">${p.count > 1 ? `<span class="chip">${p.count} processus</span>` : ''}${p.windowed ? '' : '<span class="chip">arrière-plan</span>'}</span></div>
          <div class="faint small ellipsis" title="${esc(p.path)}">${esc(p.title || p.exe)}</div>
        </div>
        <div style="width:76px;text-align:right;flex:none"><div class="faint small">Mémoire</div><b>${fmtBytes(p.ram)}</b></div>
        <div class="pm-cpu" style="width:58px;text-align:right;flex:none"><div class="faint small">CPU</div><b>${nf1.format(p.cpu)} %</b></div>
        <div class="actions" style="opacity:1;flex:none">
          <button class="btn" data-pm="stop" data-key="${esc(p.key)}" title="Ferme proprement, comme le ✕ de sa fenêtre (il peut demander d'enregistrer)">${icon('stop')} Arrêter</button>
          <button class="btn ghost danger" data-pm="forceStop" data-key="${esc(p.key)}" title="Arrêt immédiat : le travail non enregistré est perdu. Utile si le programme ne répond plus.">Forcer</button>
        </div>
      </div>`).join('');
  },
  renderStopped() {
    const el = document.getElementById('pm-stopped');
    if (!el) return;
    const st = modState('processes').stopped || [];
    el.innerHTML = st.length ? `
      <div class="section-title">Arrêtés récemment</div>
      <div class="list">${st.map(x => `
        <div class="list-item">${icon('stop')}
          <div class="main"><b>${esc(x.name)}</b><div class="faint small ellipsis">${esc(x.path)} · ${fmtAgo(x.time)}</div></div>
          <button class="btn primary" data-pm="relaunch" data-key="${esc(x.path.toLowerCase())}">${icon('play')} Relancer</button>
          <button class="btn ghost icon-only" data-pm="forget" data-key="${esc(x.path.toLowerCase())}" title="Retirer de la liste">${icon('close')}</button>
        </div>`).join('')}</div>` : '';
  },
};

// ------------------------------------------------------------------ Statistiques

const EVENT_LABELS = {
  copy: ['Textes copiés', 'clipboard'], ocr: ['Textes lus (OCR)', 'scan'], screenshot: ["Captures d'écran", 'camera'],
  enterFix: ['Fautes Enter corrigées', 'keyboard'], appGroup: ["Groupes d'apps lancés", 'apps'],
  colorPick: ['Couleurs copiées', 'drop'], placeLaunch: ['Lancements par lieu', 'place'], programStop: ['Programmes arrêtés', 'stop'], programRelaunch: ['Programmes relancés', 'update'],
};
let statsRange = 'today';

function fmtHM(sec) {
  sec = Math.round(sec || 0);
  const h = Math.floor(sec / 3600), m = Math.round(sec % 3600 / 60);
  return h ? `${h} h ${String(m).padStart(2, '0')}` : `${m} min`;
}
const hourLabel = h => new Date(h * 3600 * 1000).toLocaleString('fr-CA', { weekday: 'short', hour: '2-digit', minute: '2-digit' });

const StatsPage = {
  render() {
    return `
    <div class="grid stat-tiles" id="st-tiles"></div>
    <div class="grid grid-2" style="margin-top:12px">
      <div class="card">
        <h2>Temps d'écran — 7 derniers jours</h2>
        <div id="st-days"></div>
      </div>
      <div class="card">
        <div class="row wrap" style="margin-bottom:10px"><h2 style="margin:0">Apps les plus utilisées</h2><span class="spacer"></span>
          <div class="tabs" style="margin:0"><button class="tab" data-range="today">Aujourd'hui</button><button class="tab" data-range="week">7 jours</button></div></div>
        <div id="st-apps"></div>
      </div>
    </div>
    <div class="card" style="margin-top:12px">
      <div class="row wrap"><h2 style="margin:0">Processeur et mémoire — 48 dernières heures</h2><span class="spacer"></span>
        <div class="legend"><span><i style="background:var(--accent)"></i>Processeur</span><span><i style="background:#f2a93b"></i>Mémoire</span></div></div>
      <div id="st-hours" style="margin-top:10px"></div>
    </div>
    <div class="row" style="margin:18px 0 8px"><div class="section-title" style="margin:0" id="st-since">Depuis le début</div><span class="spacer"></span>
      <button class="btn ghost" id="st-reset">${icon('trash')} Remettre à zéro</button></div>
    <div class="grid stat-tiles" id="st-totals"></div>
    <p class="faint small" style="margin-top:14px">🔒 Ces statistiques restent sur ton PC. Le temps d'écran ne compte que quand tu utilises le clavier ou la souris.</p>`;
  },
  bind(root) {
    $$('[data-range]', root).forEach(b => b.addEventListener('click', () => { statsRange = b.dataset.range; this.update(); }));
    $('#st-reset', root).addEventListener('click', () => { if (confirm('Effacer toutes les statistiques ?')) moduleAction('stats', 'reset'); });
    this.update();
  },
  update() {
    const s = modState('stats');
    const tiles = document.getElementById('st-tiles');
    if (!tiles) return;
    const days = s.days || [];
    const todayKey = dateKey(new Date());
    const today = days.find(d => d.date === todayKey) || { active: 0, apps: [], events: {} };
    const ev = today.events || {};
    const tile = (label, value, ic) => `<div class="card tight stat-tile">${icon(ic)}<div style="min-width:0"><div class="label ellipsis">${label}</div><div class="value">${value}</div></div></div>`;
    tiles.innerHTML = [
      tile("Temps d'écran aujourd'hui", fmtHM(today.active), 'monitor'),
      tile('Textes copiés', nf0.format(ev.copy || 0), 'clipboard'),
      tile('Captures et textes lus', nf0.format((ev.screenshot || 0) + (ev.ocr || 0)), 'camera'),
      tile('Fautes Enter corrigées', nf0.format(ev.enterFix || 0), 'keyboard'),
    ].join('');

    // Barres des 7 derniers jours (les jours sans données comptent pour 0)
    const byDate = Object.fromEntries(days.map(d => [d.date, d]));
    const list = [];
    for (let i = 6; i >= 0; i--) {
      const d = new Date(); d.setDate(d.getDate() - i);
      list.push({ label: i === 0 ? 'Auj.' : d.toLocaleDateString('fr-CA', { weekday: 'short' }).replace('.', ''), sec: byDate[dateKey(d)]?.active || 0 });
    }
    const max = Math.max(3600, ...list.map(x => x.sec));
    document.getElementById('st-days').innerHTML = `<div class="bars">${list.map(x => `
      <div class="bar-col" title="${esc(fmtHM(x.sec))}"><div class="bar-val">${x.sec ? fmtHM(x.sec) : ''}</div>
        <div class="bar"><div style="height:${(100 * x.sec / max).toFixed(1)}%"></div></div><div class="bar-label">${esc(x.label)}</div></div>`).join('')}</div>`;

    $$('[data-range]').forEach(b => b.classList.toggle('active', b.dataset.range === statsRange));
    const apps = statsRange === 'today' ? today.apps || [] : s.week || [];
    const amax = Math.max(1, ...apps.map(a => a.sec));
    document.getElementById('st-apps').innerHTML = apps.length ? `<div class="app-bars">${apps.slice(0, 8).map(a => `
      <div class="app-bar"><div class="row"><span class="ellipsis" style="flex:1">${esc(a.name)}</span><b>${fmtHM(a.sec)}</b></div>
        <div class="app-bar-track"><div style="width:${(100 * a.sec / amax).toFixed(1)}%"></div></div></div>`).join('')}</div>`
      : '<div class="empty">Pas encore de données. Reviens dans quelques minutes !</div>';

    const hours = s.hours || [];
    document.getElementById('st-hours').innerHTML = hours.length > 1
      ? chartSVG([{ data: hours.map(h => h.cpu) }, { data: hours.map(h => h.ram), cls: 'alt' }], { points: 48, height: 150 })
        + `<div class="row faint small" style="justify-content:space-between;margin-top:4px"><span>${esc(hourLabel(hours[0].hour))}</span><span>maintenant</span></div>`
      : '<div class="empty">Le graphique se remplit heure par heure.</div>';

    const totals = s.totals || {};
    document.getElementById('st-since').textContent = s.since
      ? `Depuis le ${new Date(s.since * 1000).toLocaleDateString('fr-CA', { day: 'numeric', month: 'long' })}` : 'Depuis le début';
    document.getElementById('st-totals').innerHTML = Object.entries(EVENT_LABELS).map(([k, [label, ic]]) => tile(label, nf0.format(totals[k] || 0), ic)).join('');
  },
};
const dateKey = d => `${d.getFullYear()}-${String(d.getMonth() + 1).padStart(2, '0')}-${String(d.getDate()).padStart(2, '0')}`;

// ------------------------------------------------------------------ Garde Enter

const EnterGuardPage = {
  render() {
    const s = modState('enter_guard');
    return `
    <div class="card">
      <div class="hero" style="grid-template-columns:auto 1fr">
        <div style="text-align:center;padding:0 20px">
          <div class="keycap ${s.capturing ? 'listening' : ''}" style="min-width:64px;height:64px;font-size:28px">${s.capturing ? '…' : esc(s.keyName || '?')}</div>
          <div class="muted" style="margin-top:12px">Touche surveillée</div>
        </div>
        <div>
          <div class="label">Frappes accidentelles corrigées</div>
          <div class="value big" style="font-size:40px">${nf0.format(s.corrections || 0)}</div>
          <div class="row wrap" style="margin-top:18px">
            ${s.capturing
              ? `<span class="pill warn"><span class="led"></span>Appuie sur la touche à surveiller… (Échap pour annuler)</span>
                 <button class="btn" id="eg-cancel">Annuler</button>`
              : `<button class="btn primary" id="eg-capture">${icon('keyboard')} Choisir une autre touche</button>
                 ${s.isDefaultKey ? '' : '<button class="btn" id="eg-reset-key">Touche par défaut</button>'}`}
          </div>
        </div>
      </div>
    </div>

    <div class="grid grid-2">
      <div class="card tight">
        <h3><kbd class="small">${esc(s.keyName || '?')}</kbd> puis <kbd class="small">Enter</kbd> trop vite</h3>
        <div class="muted">Enter est retenu, la touche est effacée, puis Enter est envoyé. Le message part sans le caractère en trop.</div>
      </div>
      <div class="card tight">
        <h3><kbd class="small">Enter</kbd> puis <kbd class="small">${esc(s.keyName || '?')}</kbd> trop vite</h3>
        <div class="muted">La touche est simplement bloquée : elle n'apparaît jamais.</div>
      </div>
    </div>

    <div class="section-title">Réglages</div>
    <div class="setting">
      ${icon('info')}
      <div class="text"><div class="title">Délai de détection</div>
        <div class="desc">En dessous de ce délai entre les deux touches, c'est considéré comme un accident. Les touches enfoncées en même temps sont toujours corrigées.</div></div>
      <div style="width:260px" class="row"><input type="range" id="eg-threshold" min="20" max="250" step="5" value="${s.thresholdMs || 80}"><b id="eg-threshold-v" style="width:64px;text-align:right">${s.thresholdMs || 80} ms</b></div>
    </div>
    <div class="setting" style="align-items:flex-start">
      ${icon('apps')}
      <div class="text"><div class="title">Apps exclues</div>
        <div class="desc">Une par ligne, ex. <span class="mono">valorant.exe</span>. La Garde Enter ne fait rien dans ces apps (utile pour les jeux).</div></div>
      <textarea id="eg-exclusions" style="width:260px" placeholder="jeu.exe">${esc((s.exclusions || []).join('\n'))}</textarea>
    </div>
    <div class="setting">
      ${icon('trash')}
      <div class="text"><div class="title">Remettre le compteur à zéro</div></div>
      <button class="btn" id="eg-reset-stats">Réinitialiser</button>
    </div>`;
  },
  bind(root) {
    const r = $('#eg-threshold', root);
    setRangeFill(r);
    r.addEventListener('input', () => { $('#eg-threshold-v').textContent = r.value + ' ms'; setRangeFill(r); });
    r.addEventListener('change', () => moduleAction('enter_guard', 'setThreshold', { value: +r.value }));
    $('#eg-exclusions', root).addEventListener('change', e => {
      const list = e.target.value.split(/\n+/).map(x => x.trim().toLowerCase()).filter(Boolean);
      moduleAction('enter_guard', 'setExclusions', { list });
      toast('Apps exclues enregistrées');
    });
    $('#eg-capture', root)?.addEventListener('click', () => moduleAction('enter_guard', 'captureKey'));
    $('#eg-cancel', root)?.addEventListener('click', () => moduleAction('enter_guard', 'cancelCapture'));
    $('#eg-reset-key', root)?.addEventListener('click', () => moduleAction('enter_guard', 'resetKey'));
    $('#eg-reset-stats', root).addEventListener('click', () => moduleAction('enter_guard', 'resetStats'));
  },
};

// ------------------------------------------------------------------ Presse-papiers

let clipQuery = '';
const ClipboardPage = {
  render() {
    const s = modState('clipboard');
    return `
    <div id="clip-hotkey"></div>
    <div class="row" style="margin-bottom:14px">
      <div style="position:relative;flex:1">
        <input type="search" id="clip-search" placeholder="Rechercher dans l'historique…" value="${esc(clipQuery)}" style="padding-left:38px">
        <span style="position:absolute;left:11px;top:10px;color:var(--text-3)">${icon('search')}</span>
      </div>
      <button class="btn" id="clip-clear">${icon('trash')} Tout effacer</button>
    </div>
    <div class="list" id="clip-list"></div>

    <div class="section-title">Réglages</div>
    <div class="setting">${icon('folder')}
      <div class="text"><div class="title">Garder l'historique après un redémarrage</div>
        <div class="desc">Sinon, seuls les éléments épinglés sont conservés. Les mots de passe copiés depuis un gestionnaire sont toujours ignorés.</div></div>
      <input type="checkbox" class="switch" id="clip-keep" ${s.keepAfterRestart ? 'checked' : ''}>
    </div>
    <div class="setting">${icon('clipboard')}
      <div class="text"><div class="title">Nombre d'éléments maximum</div></div>
      <select id="clip-max" style="width:120px">${[25, 50, 100, 200, 500].map(n => `<option ${n === s.maxItems ? 'selected' : ''}>${n}</option>`).join('')}</select>
    </div>`;
  },
  bind(root) {
    bindHotkeyCard($('#clip-hotkey', root), 'clipboard', () => this.renderHotkey());
    $('#clip-hotkey', root).addEventListener('change', e => {
      if (e.target.id === 'hk-paste') moduleAction('clipboard', 'setAutoPaste', { value: e.target.checked });
    });
    this.renderHotkey();
    $('#clip-search', root).addEventListener('input', e => { clipQuery = e.target.value; ClipboardPage.renderList(); });
    $('#clip-clear', root).addEventListener('click', () => moduleAction('clipboard', 'clear'));
    $('#clip-keep', root).addEventListener('change', e => moduleAction('clipboard', 'setKeep', { value: e.target.checked }));
    $('#clip-max', root).addEventListener('change', e => moduleAction('clipboard', 'setMax', { value: +e.target.value }));
    $('#clip-list', root).addEventListener('click', e => {
      const item = e.target.closest('[data-id]');
      if (!item) return;
      const id = +item.dataset.id;
      const act = e.target.closest('[data-act]')?.dataset.act || 'copy';
      moduleAction('clipboard', act, { id });
      if (act === 'copy') toast('Copié !');
    });
    this.renderList();
  },
  update() { this.renderHotkey(); this.renderList(); },
  renderHotkey() {
    const el = document.getElementById('clip-hotkey');
    if (!el) return;
    el.innerHTML = hotkeyCardHTML('clipboard', {
      title: 'Fenêtre rapide',
      desc: "Ce raccourci ouvre une petite fenêtre avec ton historique, n'importe où dans Windows. Choisis un élément : il est collé là où tu écrivais.",
      extra: `<label class="row"><input type="checkbox" class="switch" id="hk-paste" ${modState('clipboard').autoPaste ? 'checked' : ''}> Coller automatiquement</label>`,
    });
  },
  renderList() {
    const el = document.getElementById('clip-list');
    if (!el) return;
    const q = clipQuery.trim().toLowerCase();
    const items = (modState('clipboard').items || []).filter(i => !q || i.text.toLowerCase().includes(q));
    const pinned = items.filter(i => i.pinned), rest = items.filter(i => !i.pinned);
    if (!items.length) {
      el.innerHTML = `<div class="card empty">${icon('clipboard')}<div>${q ? 'Aucun résultat' : 'Copie du texte (Ctrl + C) : il apparaîtra ici.'}</div></div>`;
      return;
    }
    el.innerHTML = [...pinned, ...rest].map(i => `
      <div class="list-item clip-item ${i.pinned ? 'pinned' : ''}" data-id="${i.id}">
        <div class="main" title="Cliquer pour copier">
          <div class="clip-text" data-act="copy">${esc(i.text.length > 600 ? i.text.slice(0, 600) + '…' : i.text)}</div>
          <div class="clip-meta">${i.pinned ? '📌 Épinglé · ' : ''}${fmtAgo(i.time)} · ${nf0.format(i.text.length)} caractère${i.text.length > 1 ? 's' : ''}</div>
        </div>
        <div class="actions">
          <button class="btn ghost icon-only" data-act="copy" title="Copier">${icon('copy')}</button>
          <button class="btn ghost icon-only" data-act="pin" title="${i.pinned ? 'Désépingler' : 'Épingler'}" style="${i.pinned ? 'color:var(--accent)' : ''}">${icon('pin')}</button>
          <button class="btn ghost icon-only" data-act="delete" title="Supprimer">${icon('trash')}</button>
        </div>
      </div>`).join('');
  },
};

// ------------------------------------------------------------------ Texte à l'écran (OCR)

const OcrPage = {
  render() {
    return `
    <div class="grid grid-2">
      <div class="card capture-card">
        <div class="row" style="gap:12px">
          <div class="ocr-illus">${icon('camera')}</div>
          <div style="flex:1;min-width:0"><h2 style="margin:0">Capture d'écran</h2>
            <div class="muted small">L'image est copiée (colle-la avec Ctrl + V) et enregistrée en PNG.</div></div>
        </div>
        <button class="btn primary big" data-capture="shot">${icon('camera')} Capturer une zone</button>
        <div id="shot-hotkey"></div>
        <label class="row small"><input type="checkbox" class="switch" id="shot-save" ${modState('ocr').shotSave ? 'checked' : ''}> Enregistrer dans Images\\Captures ToolBox</label>
      </div>
      <div class="card capture-card">
        <div class="row" style="gap:12px">
          <div class="ocr-illus">${icon('scan')}</div>
          <div style="flex:1;min-width:0"><h2 style="margin:0">Lire le texte (OCR)</h2>
            <div class="muted small">Tesseract lit le texte de la zone et le copie. Marche sur les images, vidéos, PDF scannés…</div></div>
        </div>
        <button class="btn primary big" data-capture="text">${icon('scan')} Lire une zone</button>
        <div id="ocr-hotkey"></div>
        <div id="ocr-status"></div>
      </div>
    </div>

    <div class="row" style="margin:18px 0 8px"><div class="section-title" style="margin:0">Dernières captures</div><span class="spacer"></span>
      <button class="btn ghost" data-shot-act="folder">${icon('folder')} Ouvrir le dossier</button></div>
    <div class="shot-grid" id="shot-list"></div>

    <div class="row" style="margin:18px 0 8px"><div class="section-title" style="margin:0">Derniers textes lus</div><span class="spacer"></span>
      <button class="btn ghost" id="ocr-clear">${icon('trash')} Tout effacer</button></div>
    <div class="list" id="ocr-history"></div>

    <div class="section-title">Langues du texte</div>
    <div class="card tight" id="ocr-langs"></div>
    <div class="section-title">Réglages</div>
    <div class="setting">${icon('convert')}
      <div class="text"><div class="title">Texte sur une seule ligne</div><div class="desc">Pratique pour coller dans une barre de recherche ou un formulaire.</div></div>
      <input type="checkbox" class="switch" id="ocr-single" ${modState('ocr').singleLine ? 'checked' : ''}></div>
    <div class="setting">${icon('info')}
      <div class="text"><div class="title">Afficher une notification</div><div class="desc">Un aperçu apparaît près de l'horloge après chaque capture.</div></div>
      <input type="checkbox" class="switch" id="ocr-notify" ${modState('ocr').notify ? 'checked' : ''}></div>`;
  },
  bind(root) {
    $$('[data-capture]', root).forEach(b => b.addEventListener('click', () => send({ type: 'ocrCapture', shot: b.dataset.capture === 'shot' })));
    bindHotkeyCard($('#ocr-hotkey', root), 'ocr', () => this.renderHotkeys());
    bindHotkeyCard($('#shot-hotkey', root), 'ocr', () => this.renderHotkeys(), 'shot');
    $('#shot-save', root).addEventListener('change', e => moduleAction('ocr', 'setShotSave', { value: e.target.checked }));
    $('#ocr-single', root).addEventListener('change', e => moduleAction('ocr', 'setSingleLine', { value: e.target.checked }));
    $('#ocr-notify', root).addEventListener('change', e => moduleAction('ocr', 'setNotify', { value: e.target.checked }));
    $('#ocr-clear', root).addEventListener('click', () => moduleAction('ocr', 'clear'));
    root.addEventListener('click', e => {
      const b = e.target.closest('[data-shot-act]');
      if (!b) return;
      const act = { folder: 'openShotFolder', open: 'openShot', show: 'showShot', remove: 'deleteShot' }[b.dataset.shotAct];
      moduleAction('ocr', act, { id: +b.dataset.id || 0 });
    });
    $('#ocr-langs', root).addEventListener('click', e => {
      if (e.target.closest('#ocr-download')) { moduleAction('ocr', 'downloadLangs'); return; }
      const chip = e.target.closest('[data-lang]');
      if (!chip) return;
      const langs = modState('ocr').langs || [];
      let sel = langs.filter(l => l.selected).map(l => l.code);
      sel = sel.includes(chip.dataset.lang) ? sel.filter(c => c !== chip.dataset.lang) : [...sel, chip.dataset.lang];
      if (!sel.length) return toast('Garde au moins une langue');
      moduleAction('ocr', 'setLangs', { list: sel });
    });
    $('#ocr-history', root).addEventListener('click', e => {
      const b = e.target.closest('[data-ocr-act]');
      if (!b) return;
      moduleAction('ocr', b.dataset.ocrAct, { id: +b.dataset.id });
      if (b.dataset.ocrAct === 'copy') toast('Copié !');
    });
    this.update();
  },
  update() {
    const s = modState('ocr');
    const st = document.getElementById('ocr-status');
    if (!st) return;
    st.innerHTML = {
      downloading: `<span class="pill warn"><span class="led"></span>Téléchargement de la langue : ${esc(s.detail)}</span>`,
      reading: `<span class="pill warn"><span class="led"></span>Lecture en cours…</span>`,
      error: `<div class="banner" style="margin:0">${icon('info')}<div class="text">${esc(s.detail)}</div></div>`,
    }[s.status] || `<span class="pill ok"><span class="led"></span>Prêt${s.engineLoaded ? ' · moteur en mémoire' : ''}</span>`;
    this.renderHotkeys();
    this.renderShots();
    this.renderLangs();
    this.renderHistory();
  },
  renderHotkeys() {
    const a = document.getElementById('shot-hotkey'), b = document.getElementById('ocr-hotkey');
    if (a) a.innerHTML = hotkeyCardHTML('ocr', { prefix: 'shot', compact: true, desc: 'Raccourci de la capture' });
    if (b) b.innerHTML = hotkeyCardHTML('ocr', { compact: true, desc: 'Raccourci de la lecture' });
  },
  renderShots() {
    const el = document.getElementById('shot-list');
    if (!el) return;
    const shots = modState('ocr').shots || [];
    el.innerHTML = shots.length ? shots.slice(0, 12).map(x => `
      <div class="shot-item">
        <div class="shot-thumb" ${x.exists ? `data-shot-act="open" data-id="${x.id}" title="Ouvrir"` : ''}>${icon('camera')}<span>${x.w} × ${x.h}</span></div>
        <div class="shot-meta"><div class="ellipsis small" title="${esc(x.path)}">${x.path ? esc(fileName(x.path)) : 'Copiée seulement'}</div>
          <div class="faint small">${fmtAgo(x.time)}</div></div>
        <div class="row" style="gap:2px">
          ${x.exists ? `<button class="btn ghost icon-only" data-shot-act="show" data-id="${x.id}" title="Afficher dans le dossier">${icon('folder')}</button>` : ''}
          <button class="btn ghost icon-only" data-shot-act="remove" data-id="${x.id}" title="Retirer de la liste">${icon('close')}</button>
        </div>
      </div>`).join('') : `<div class="card empty" style="grid-column:1/-1">${icon('camera')}<div>Tes captures apparaîtront ici.</div></div>`;
  },
  renderLangs() {
    const el = document.getElementById('ocr-langs');
    if (!el) return;
    const langs = modState('ocr').langs || [];
    const missing = langs.filter(l => l.selected && !l.installed);
    const mb = missing.reduce((a, l) => a + l.size, 0) / 1e6;
    el.innerHTML = `
      <div class="row wrap" style="gap:6px">${langs.map(l => `
        <button class="tab ${l.selected ? 'active' : ''}" data-lang="${l.code}" title="${l.installed ? 'Installée' : 'Sera téléchargée (' + nf1.format(l.size / 1e6) + ' Mo)'}">
          ${esc(l.name)} ${l.installed ? '✓' : '↓'}</button>`).join('')}</div>
      <div class="row wrap" style="margin-top:10px">
        <span class="muted small" style="flex:1">${missing.length
          ? `${missing.map(l => l.name).join(', ')} : ${nf1.format(mb)} Mo à télécharger une seule fois (fait automatiquement à la première lecture).`
          : 'Toutes les langues choisies sont installées. Moins de langues = lecture plus rapide.'}</span>
        ${missing.length ? `<button class="btn" id="ocr-download">${icon('update')} Télécharger maintenant</button>` : ''}
      </div>`;
  },
  renderHistory() {
    const el = document.getElementById('ocr-history');
    if (!el) return;
    const h = modState('ocr').history || [];
    el.innerHTML = h.length ? h.map(e => `
      <div class="list-item">
        <div class="main"><div class="clip-text" style="user-select:text">${esc(e.text.length > 600 ? e.text.slice(0, 600) + '…' : e.text)}</div>
          <div class="clip-meta">${fmtAgo(e.time)} · lu en ${nf1.format(e.ms / 1000)} s · ${nf0.format(e.text.length)} caractères</div></div>
        <div class="actions">
          <button class="btn ghost icon-only" data-ocr-act="copy" data-id="${e.id}" title="Copier">${icon('copy')}</button>
          <button class="btn ghost icon-only" data-ocr-act="delete" data-id="${e.id}" title="Supprimer">${icon('trash')}</button>
        </div>
      </div>`).join('') : `<div class="card empty">${icon('scan')}<div>Les textes lus apparaîtront ici.</div></div>`;
  },
};

// ------------------------------------------------------------------ Pipette

const ColorPage = {
  render() {
    return `
    <div class="card">
      <div class="row wrap" style="gap:18px">
        <div class="color-big" id="color-big"></div>
        <div style="flex:1;min-width:220px">
          <h2 style="margin:0 0 4px">Choisir une couleur à l'écran</h2>
          <div class="muted small">Une loupe suit ta souris : clique sur le pixel voulu (les flèches visent au pixel près). La couleur est copiée.</div>
          <div class="tabs" style="margin:12px 0 0" id="color-formats">
            ${['hex', 'rgb', 'hsl'].map(f => `<button class="tab" data-format="${f}">${f.toUpperCase()}</button>`).join('')}
          </div>
        </div>
        <button class="btn primary big" id="color-go">${icon('drop')} Choisir une couleur</button>
      </div>
    </div>
    <div id="color-hotkey"></div>
    <div class="row" style="margin:18px 0 8px"><div class="section-title" style="margin:0">Dernières couleurs</div><span class="spacer"></span>
      <button class="btn ghost" id="color-clear">${icon('trash')} Tout effacer</button></div>
    <div class="swatch-grid" id="color-list"></div>`;
  },
  bind(root) {
    $('#color-go', root).addEventListener('click', () => send({ type: 'colorPick' }));
    bindHotkeyCard($('#color-hotkey', root), 'color', () => this.renderHotkey());
    $('#color-clear', root).addEventListener('click', () => moduleAction('color', 'clear'));
    $('#color-formats', root).addEventListener('click', e => {
      const b = e.target.closest('[data-format]');
      if (b) moduleAction('color', 'setFormat', { value: b.dataset.format });
    });
    $('#color-list', root).addEventListener('click', e => {
      const del = e.target.closest('[data-color-del]');
      if (del) { moduleAction('color', 'delete', { id: +del.dataset.colorDel }); return; }
      const b = e.target.closest('[data-color-copy]');
      if (!b) return;
      moduleAction('color', 'copy', { id: +b.dataset.colorCopy, format: b.dataset.format || undefined });
      toast('Copié : ' + b.dataset.text);
    });
    this.update();
  },
  update() {
    const s = modState('color');
    const big = document.getElementById('color-big');
    if (!big) return;
    const last = (s.history || [])[0];
    big.style.background = last ? last.hex : 'repeating-conic-gradient(#333 0 25%, #2a2a2a 0 50%) 0 0 / 16px 16px';
    big.innerHTML = last ? `<span>${esc(last[s.format] || last.hex)}</span>` : '';
    $$('#color-formats [data-format]').forEach(b => b.classList.toggle('active', b.dataset.format === s.format));
    this.renderHotkey();
    const list = s.history || [];
    document.getElementById('color-list').innerHTML = list.length ? list.map(c => `
      <div class="swatch">
        <div class="swatch-color" style="background:${esc(c.hex)}" data-color-copy="${c.id}" data-text="${esc(c[s.format])}" title="Copier ${esc(c[s.format])}"></div>
        <div class="swatch-info">
          ${['hex', 'rgb', 'hsl'].map(f => `<button class="swatch-code${f === s.format ? ' main' : ''}" data-color-copy="${c.id}" data-format="${f}" data-text="${esc(c[f])}" title="Copier">${esc(c[f])}</button>`).join('')}
        </div>
        <button class="btn ghost icon-only swatch-del" data-color-del="${c.id}" title="Retirer">${icon('close')}</button>
      </div>`).join('') : `<div class="card empty" style="grid-column:1/-1">${icon('drop')}<div>Les couleurs choisies apparaîtront ici.</div></div>`;
  },
  renderHotkey() {
    const el = document.getElementById('color-hotkey');
    if (el) el.innerHTML = hotkeyCardHTML('color', { title: 'Raccourci', desc: "Ouvre la pipette depuis n'importe quelle app." });
  },
};

// ------------------------------------------------------------------ Raccourcis clavier (bloc réutilisable)

let hotkeyCapturing = null;   // { id, prefix } : raccourci en cours de choix
let hotkeyRerender = null;
const hkKey = (prefix, name) => prefix ? `${prefix}_${name}` : name;
const isCapturing = (id, prefix = '') => !!hotkeyCapturing && hotkeyCapturing.id === id && hotkeyCapturing.prefix === prefix;

function hotkeyCardHTML(id, { title, desc, extra = '', prefix = '', compact = false }) {
  const s = modState(id);
  const capturing = isCapturing(id, prefix);
  const err = s[hkKey(prefix, 'hotkeyError')];
  return `
    ${err && !capturing ? `<div class="banner">${icon('info')}<div class="text">${esc(err)}</div></div>` : ''}
    <div class="${compact ? '' : 'card'}">
      <div class="row wrap" style="gap:14px">
        <div class="keycap ${capturing ? 'listening' : ''}" style="font-size:14px;height:38px;min-width:96px">${capturing ? 'Appuie…' : esc(s[hkKey(prefix, 'hotkeyLabel')] || '—')}</div>
        <div style="flex:1;min-width:180px">${title ? `<h3>${esc(title)}</h3>` : ''}<div class="muted">${esc(desc)}</div></div>
        ${capturing
          ? `<div class="stack" style="align-items:flex-end;gap:6px"><span class="pill warn"><span class="led"></span>Appuie sur la combinaison</span><button class="btn" data-hk="cancel">Annuler</button></div>`
          : `<button class="btn" data-hk="capture">${icon('keyboard')} Changer</button>`}
      </div>
      <div class="row wrap" style="margin-top:12px;gap:22px">
        <label class="row"><input type="checkbox" class="switch" data-hk-enabled ${s[hkKey(prefix, 'hotkeyEnabled')] ? 'checked' : ''}> Raccourci activé</label>
        ${extra}
      </div>
    </div>`;
}

function bindHotkeyCard(el, id, rerender, prefix = '') {
  el.addEventListener('click', e => {
    const b = e.target.closest('[data-hk]');
    if (!b) return;
    if (b.dataset.hk === 'capture') {
      if (hotkeyCapturing) stopHotkeyCapture(true);
      hotkeyCapturing = { id, prefix }; hotkeyRerender = rerender;
      moduleAction(id, 'hotkeyCaptureStart', { hotkey: prefix });
      rerender();
    }
    if (b.dataset.hk === 'cancel') stopHotkeyCapture(true);
  });
  el.addEventListener('change', e => {
    if (e.target.matches('[data-hk-enabled]')) moduleAction(id, 'setHotkeyEnabled', { value: e.target.checked, hotkey: prefix });
  });
}

function stopHotkeyCapture(cancel) {
  const cap = hotkeyCapturing, rerender = hotkeyRerender;
  hotkeyCapturing = null; hotkeyRerender = null;
  if (cancel && cap) moduleAction(cap.id, 'hotkeyCaptureCancel', { hotkey: cap.prefix });
  if (rerender) rerender();
}

const KEY_NAMES = { Space: 'Espace', Enter: 'Entrée', Tab: 'Tab', Backspace: 'Retour', Insert: 'Inser', Delete: 'Suppr', Home: 'Début', End: 'Fin', PageUp: 'Page ↑', PageDown: 'Page ↓', ArrowUp: '↑', ArrowDown: '↓', ArrowLeft: '←', ArrowRight: '→' };
document.addEventListener('keydown', e => {
  if (!hotkeyCapturing) return;
  e.preventDefault();
  e.stopPropagation();
  if (e.key === 'Escape') return stopHotkeyCapture(true);
  if (['Control', 'Alt', 'Shift', 'Meta', 'AltGraph'].includes(e.key)) return;  // attendre la vraie touche
  const isF = /^F\d{1,2}$/.test(e.key);
  if (!(e.ctrlKey || e.altKey || e.metaKey) && !isF) { toast('Ajoute Ctrl, Alt ou Windows (ex. Ctrl + Alt + V)'); return; }
  const mods = (e.altKey ? 1 : 0) | (e.ctrlKey ? 2 : 0) | (e.shiftKey ? 4 : 0) | (e.metaKey ? 8 : 0);
  const keyLabel = isF ? e.key : KEY_NAMES[e.code] || (e.code.startsWith('Key') ? e.code.slice(3) : e.code.startsWith('Digit') ? e.code.slice(5) : e.key.toUpperCase());
  const label = [e.ctrlKey && 'Ctrl', e.altKey && 'Alt', e.shiftKey && 'Maj', e.metaKey && 'Win', keyLabel].filter(Boolean).join(' + ');
  const cap = hotkeyCapturing;
  stopHotkeyCapture(false);
  moduleAction(cap.id, 'setHotkey', { mods, vk: e.keyCode, label, hotkey: cap.prefix });
  toast('Raccourci : ' + label);
}, true);

// ------------------------------------------------------------------ Fenêtre rapide (popup du presse-papiers)

const Popup = {
  q: '',
  sel: 0,
  built: false,
  items() {
    const q = fold(this.q.trim());
    const all = modState('clipboard').items || [];
    const list = all.filter(i => !q || fold(i.text).includes(q));
    return [...list.filter(i => i.pinned), ...list.filter(i => !i.pinned)];
  },
  build() {
    const s = modState('clipboard');
    $('#page').innerHTML = `
      <div class="pop">
        <div class="pop-head">${icon('clipboard')}<b>Presse-papiers</b><span class="spacer"></span>
          <button class="btn ghost icon-only" id="pop-open" title="Ouvrir ToolBox">${icon('apps')}</button>
          <button class="btn ghost icon-only" id="pop-close" title="Fermer (Échap)">${icon('close')}</button></div>
        <div class="pop-search">${icon('search')}<input type="search" id="pop-q" placeholder="Rechercher…" autocomplete="off"></div>
        <div class="pop-list" id="pop-list"></div>
        <div class="pop-foot">↑ ↓ choisir · Entrée ${s.autoPaste ? 'coller' : 'copier'} · Échap fermer</div>
      </div>`;
    $('#pop-q').addEventListener('input', e => { this.q = e.target.value; this.sel = 0; this.renderList(); });
    $('#pop-close').addEventListener('click', () => send({ type: 'popupClose' }));
    $('#pop-open').addEventListener('click', () => send({ type: 'openMain' }));
    $('#pop-list').addEventListener('click', e => {
      const pin = e.target.closest('[data-pin]');
      if (pin) { moduleAction('clipboard', 'pin', { id: +pin.dataset.pin }); return; }
      const it = e.target.closest('[data-id]');
      if (it) this.pick(+it.dataset.id);
    });
    $('#pop-list').addEventListener('mousemove', e => {
      const it = e.target.closest('[data-idx]');
      if (it && +it.dataset.idx !== this.sel) { this.sel = +it.dataset.idx; this.highlight(); }
    });
    document.addEventListener('keydown', e => {
      const n = this.items().length;
      if (e.key === 'Escape') { e.preventDefault(); send({ type: 'popupClose' }); }
      else if (e.key === 'ArrowDown') { e.preventDefault(); this.sel = Math.min(n - 1, this.sel + 1); this.highlight(true); }
      else if (e.key === 'ArrowUp') { e.preventDefault(); this.sel = Math.max(0, this.sel - 1); this.highlight(true); }
      else if (e.key === 'Enter') { e.preventDefault(); const it = this.items()[this.sel]; if (it) this.pick(it.id); }
    });
    this.built = true;
  },
  pick(id) { send({ type: 'clipPaste', id }); },
  render() {
    if (!this.built) this.build();
    this.renderList();
  },
  renderList() {
    const el = $('#pop-list');
    if (!el) return;
    const items = this.items();
    if (this.sel >= items.length) this.sel = Math.max(0, items.length - 1);
    el.innerHTML = items.length ? items.map((i, k) => `
      <div class="pop-item${k === this.sel ? ' sel' : ''}${i.pinned ? ' pinned' : ''}" data-id="${i.id}" data-idx="${k}">
        <div class="pop-text">${esc(i.text.length > 300 ? i.text.slice(0, 300) + '…' : i.text)}</div>
        <button class="btn ghost icon-only pop-pin" data-pin="${i.id}" title="${i.pinned ? 'Désépingler' : 'Épingler'}">${icon('pin')}</button>
      </div>`).join('') : `<div class="empty">${this.q ? 'Aucun résultat' : 'Rien de copié pour l\'instant'}</div>`;
  },
  highlight(scroll) {
    $$('.pop-item').forEach((el, k) => el.classList.toggle('sel', k === this.sel));
    if (scroll) $('.pop-item.sel')?.scrollIntoView({ block: 'nearest' });
  },
  shown() {
    this.q = ''; this.sel = 0;
    const q = $('#pop-q');
    if (q) { q.value = ''; q.focus(); }
    this.renderList();
  },
};

// ------------------------------------------------------------------ Convertisseur

const UNITS = {
  length: { label: 'Longueur', units: { mm: ['Millimètre', 0.001], cm: ['Centimètre', 0.01], m: ['Mètre', 1], km: ['Kilomètre', 1000], in: ['Pouce', 0.0254], ft: ['Pied', 0.3048], yd: ['Verge', 0.9144], mi: ['Mille', 1609.344] }, def: ['cm', 'in'] },
  mass: { label: 'Masse', units: { mg: ['Milligramme', 1e-6], g: ['Gramme', 0.001], kg: ['Kilogramme', 1], t: ['Tonne', 1000], oz: ['Once', 0.028349523125], lb: ['Livre', 0.45359237] }, def: ['kg', 'lb'] },
  temp: { label: 'Température', units: { c: ['Celsius (°C)'], f: ['Fahrenheit (°F)'], k: ['Kelvin (K)'] }, def: ['c', 'f'] },
  volume: { label: 'Volume', units: { ml: ['Millilitre', 0.001], l: ['Litre', 1], m3: ['Mètre cube', 1000], tsp: ['Cuillère à thé', 0.00492892], tbsp: ['Cuillère à soupe', 0.0147868], cup: ['Tasse (US)', 0.2365882], gal: ['Gallon (US)', 3.785411784] }, def: ['l', 'gal'] },
  speed: { label: 'Vitesse', units: { ms: ['Mètre/seconde', 1], kmh: ['Kilomètre/heure', 1 / 3.6], mph: ['Mille/heure', 0.44704], kn: ['Nœud', 0.514444] }, def: ['kmh', 'mph'] },
  area: { label: 'Aire', units: { cm2: ['cm²', 1e-4], m2: ['m²', 1], ha: ['Hectare', 1e4], km2: ['km²', 1e6], ft2: ['pi²', 0.09290304], ac: ['Acre', 4046.8564224] }, def: ['m2', 'ft2'] },
  data: { label: 'Données', units: { o: ['Octet', 1], ko: ['Ko', 1e3], mo: ['Mo', 1e6], go: ['Go', 1e9], to: ['To', 1e12], kio: ['Kio', 1024], mio: ['Mio', 1048576], gio: ['Gio', 1073741824] }, def: ['go', 'mo'] },
  time: { label: 'Temps', units: { ms: ['Milliseconde', 0.001], s: ['Seconde', 1], min: ['Minute', 60], h: ['Heure', 3600], d: ['Jour', 86400], w: ['Semaine', 604800], y: ['Année', 31557600] }, def: ['h', 'min'] },
  money: { label: 'Devises', units: {}, def: ['CAD', 'USD'] },
};
const CURRENCIES = { CAD: 'Dollar canadien', USD: 'Dollar américain', EUR: 'Euro', GBP: 'Livre sterling', JPY: 'Yen japonais', CHF: 'Franc suisse', AUD: 'Dollar australien', MXN: 'Peso mexicain', CNY: 'Yuan chinois', KRW: 'Won sud-coréen', INR: 'Roupie indienne', BRL: 'Réal brésilien', SEK: 'Couronne suédoise' };
UNITS.money.units = Object.fromEntries(Object.entries(CURRENCIES).map(([k, v]) => [k, [`${k} — ${v}`]]));

const conv = { cat: 'length', from: null, to: null, value: '1', rates: null, ratesDate: null, loading: false };

function convert(cat, from, to, v) {
  if (cat === 'temp') {
    const c = from === 'c' ? v : from === 'f' ? (v - 32) * 5 / 9 : v - 273.15;
    return to === 'c' ? c : to === 'f' ? c * 9 / 5 + 32 : c + 273.15;
  }
  if (cat === 'money') {
    if (!conv.rates) return NaN;
    const rf = from === 'EUR' ? 1 : conv.rates[from], rt = to === 'EUR' ? 1 : conv.rates[to];
    return v / rf * rt;
  }
  const u = UNITS[cat].units;
  return v * u[from][1] / u[to][1];
}
function fmtNum(x, cat) {
  if (!isFinite(x)) return '';
  if (cat === 'money') return x.toLocaleString('fr-CA', { minimumFractionDigits: 2, maximumFractionDigits: 2 });
  if (x !== 0 && (Math.abs(x) >= 1e12 || Math.abs(x) < 1e-6)) return x.toExponential(4).replace('.', ',');
  return x.toLocaleString('fr-CA', { maximumSignificantDigits: 10, useGrouping: true });
}
function parseNum(s) { return parseFloat(String(s).replace(/\s|\u202f|\u00a0/g, '').replace(',', '.')); }

async function loadRates() {
  if (conv.loading) return;
  conv.loading = true;
  const urls = ['https://api.frankfurter.dev/v1/latest?base=EUR', 'https://api.frankfurter.app/latest?from=EUR'];
  for (const url of urls) {
    try {
      const r = await fetch(url);
      if (!r.ok) continue;
      const j = await r.json();
      conv.rates = j.rates; conv.ratesDate = j.date;
      moduleAction('converter', 'setPref', { key: 'rates', value: { rates: j.rates, date: j.date } });
      break;
    } catch { /* essai suivant */ }
  }
  conv.loading = false;
  if (current === ConverterPage) ConverterPage.compute();
}

const ConverterPage = {
  render() {
    const prefs = modState('converter');
    if (!conv.from && prefs.cat && UNITS[prefs.cat]) { conv.cat = prefs.cat; }
    if (!conv.rates && prefs.rates) { conv.rates = prefs.rates.rates; conv.ratesDate = prefs.rates.date; }
    const cat = UNITS[conv.cat];
    if (!conv.from || !cat.units[conv.from]) conv.from = cat.def[0];
    if (!conv.to || !cat.units[conv.to]) conv.to = cat.def[1];
    const opts = sel => Object.entries(cat.units).map(([k, v]) => `<option value="${k}" ${k === sel ? 'selected' : ''}>${esc(v[0])}</option>`).join('');
    return `
    <div class="tabs">${Object.entries(UNITS).map(([k, v]) => `<button class="tab ${k === conv.cat ? 'active' : ''}" data-cat="${k}">${esc(v.label)}</button>`).join('')}</div>
    <div class="card">
      <div class="conv">
        <div><div class="label">De</div><input type="text" id="cv-in" value="${esc(conv.value)}" inputmode="decimal"><select id="cv-from">${opts(conv.from)}</select></div>
        <button class="btn icon-only" id="cv-swap" title="Inverser" style="margin-bottom:46px">${icon('swap')}</button>
        <div><div class="label">Vers</div><input type="text" id="cv-out" readonly><select id="cv-to">${opts(conv.to)}</select></div>
      </div>
      <div class="muted small" id="cv-note" style="margin-top:16px"></div>
    </div>`;
  },
  bind(root) {
    $$('[data-cat]', root).forEach(b => b.addEventListener('click', () => {
      conv.cat = b.dataset.cat; conv.from = conv.to = null;
      moduleAction('converter', 'setPref', { key: 'cat', value: conv.cat });
      renderPage(true);
    }));
    $('#cv-in', root).addEventListener('input', e => { conv.value = e.target.value; this.compute(); });
    $('#cv-from', root).addEventListener('change', e => { conv.from = e.target.value; this.compute(); });
    $('#cv-to', root).addEventListener('change', e => { conv.to = e.target.value; this.compute(); });
    $('#cv-swap', root).addEventListener('click', () => {
      [conv.from, conv.to] = [conv.to, conv.from];
      $('#cv-from').value = conv.from; $('#cv-to').value = conv.to;
      this.compute();
    });
    if (conv.cat === 'money' && (!conv.rates || conv.ratesDate !== new Date().toISOString().slice(0, 10))) loadRates();
    this.compute();
    $('#cv-in', root).focus();
    $('#cv-in', root).select();
  },
  compute() {
    const out = document.getElementById('cv-out'), note = document.getElementById('cv-note');
    if (!out) return;
    const v = parseNum(conv.value);
    out.value = isNaN(v) ? '' : fmtNum(convert(conv.cat, conv.from, conv.to, v), conv.cat);
    if (conv.cat === 'money') {
      note.textContent = conv.rates
        ? `Taux de la Banque centrale européenne du ${conv.ratesDate}. 1 ${conv.from} = ${fmtNum(convert('money', conv.from, conv.to, 1), 'x')} ${conv.to}`
        : conv.loading ? 'Chargement des taux…' : 'Taux indisponibles (pas de connexion ?)';
    } else if (!isNaN(v)) {
      note.textContent = `1 ${UNITS[conv.cat].units[conv.from][0]} = ${fmtNum(convert(conv.cat, conv.from, conv.to, 1), conv.cat)} ${UNITS[conv.cat].units[conv.to][0]}`;
    } else note.textContent = '';
  },
};

// ------------------------------------------------------------------ Raccourcis d'apps

const AppLauncherPage = {
  render() {
    const s = modState('app_launcher');
    const groups = s.groups || [];
    return `
    ${s.lastEvent ? `<div class="banner info">${icon('info')}<div class="text">${esc(s.lastEvent)}</div></div>` : ''}
    <div class="grid grid-3">
      ${groups.map(g => `
        <div class="card group-card">
          <div class="row"><span class="group-emoji">${esc(g.emoji || '🚀')}</span><span class="spacer"></span>
            <button class="btn ghost icon-only" data-edit="${g.id}" title="Modifier">${icon('edit')}</button></div>
          <div><h3 style="font-size:18px">${esc(g.name || 'Sans nom')}</h3>
            <div class="group-items">${g.items.slice(0, 6).map(i => `<span class="chip ellipsis" title="${esc(i)}">${esc(/^https?:/.test(i) ? i.replace(/^https?:\/\/(www\.)?/, '').split('/')[0] : fileName(i))}</span>`).join('')}${g.items.length > 6 ? `<span class="chip">+${g.items.length - 6}</span>` : ''}</div></div>
          <button class="btn primary" data-launch="${g.id}" ${g.items.length ? '' : 'disabled'}>${icon('play')} Lancer</button>
        </div>`).join('')}
      <button class="card group-card empty" id="al-new" style="cursor:pointer;border-style:dashed;color:var(--text-2);font:inherit">
        ${icon('plus')}<div style="font-weight:600">Nouveau groupe</div><div class="small">Ex. « Devoirs » : Word + Chrome + Calculatrice</div>
      </button>
    </div>`;
  },
  bind(root) {
    $('#al-new', root).addEventListener('click', () => this.edit(null));
    $$('[data-edit]', root).forEach(b => b.addEventListener('click', () => this.edit(b.dataset.edit)));
    $$('[data-launch]', root).forEach(b => b.addEventListener('click', () => { moduleAction('app_launcher', 'launchGroup', { id: b.dataset.launch }); toast('Lancement…'); }));
  },
  edit(id) {
    const existing = (modState('app_launcher').groups || []).find(g => g.id === id);
    const g = existing ? JSON.parse(JSON.stringify(existing)) : { id: '', name: '', emoji: '🚀', items: [] };
    const dlg = $('#dialog');
    const draw = () => {
      dlg.innerHTML = `
      <div class="dlg-body">
        <h2>${existing ? 'Modifier le groupe' : 'Nouveau groupe'}</h2>
        <div class="row">
          <label class="field" style="width:90px"><span>Emoji</span><input type="text" id="g-emoji" value="${esc(g.emoji)}" maxlength="4" style="text-align:center;font-size:20px"></label>
          <label class="field" style="flex:1"><span>Nom</span><input type="text" id="g-name" value="${esc(g.name)}" placeholder="Ex. Devoirs"></label>
        </div>
        <div class="field"><span>Apps, fichiers et sites</span>
          <div class="list">${g.items.length ? g.items.map((it, i) => `
            <div class="list-item" style="padding:8px 12px">${icon(/^https?:/.test(it) ? 'link' : 'file')}
              <div class="main ellipsis" title="${esc(it)}">${esc(/^https?:/.test(it) ? it : fileName(it))}<div class="faint small ellipsis">${esc(/^https?:/.test(it) ? '' : it)}</div></div>
              <button class="btn ghost icon-only" data-rm="${i}" title="Retirer">${icon('close')}</button></div>`).join('')
            : '<div class="faint small">Aucun élément pour l\'instant.</div>'}</div>
        </div>
        <div class="row wrap">
          <button class="btn" id="g-add-file">${icon('file')} Ajouter une app ou un fichier</button>
          <div class="row" style="flex:1;min-width:240px"><input type="text" id="g-url" placeholder="https://…"><button class="btn" id="g-add-url">${icon('link')} Ajouter</button></div>
        </div>
      </div>
      <div class="dlg-foot">
        ${existing ? `<button class="btn danger left" id="g-del">${icon('trash')} Supprimer</button>` : ''}
        <button class="btn" id="g-cancel">Annuler</button>
        <button class="btn primary" id="g-save">Enregistrer</button>
      </div>`;
      const keep = () => { g.name = $('#g-name', dlg).value; g.emoji = $('#g-emoji', dlg).value; };
      $$('[data-rm]', dlg).forEach(b => b.addEventListener('click', () => { keep(); g.items.splice(+b.dataset.rm, 1); draw(); }));
      $('#g-add-file', dlg).addEventListener('click', async () => { keep(); const p = await pickFile(); if (p) { g.items.push(p); draw(); } });
      $('#g-add-url', dlg).addEventListener('click', () => {
        keep();
        let u = $('#g-url', dlg).value.trim();
        if (!u) return;
        if (!/^https?:\/\//.test(u)) u = 'https://' + u;
        g.items.push(u); draw();
      });
      $('#g-cancel', dlg).addEventListener('click', () => dlg.close());
      $('#g-del', dlg)?.addEventListener('click', () => { moduleAction('app_launcher', 'deleteGroup', { id: g.id }); dlg.close(); });
      $('#g-save', dlg).addEventListener('click', () => {
        keep();
        if (!g.name.trim()) { $('#g-name', dlg).focus(); return; }
        moduleAction('app_launcher', 'saveGroup', { group: g });
        dlg.close();
      });
    };
    draw();
    dlg.showModal();
    dlg.addEventListener('close', () => renderPage(true), { once: true });
  },
};

// ------------------------------------------------------------------ Lancement par lieu

const PlacePage = {
  render() {
    const s = modState('place_launcher');
    const rules = s.rules || [];
    const pos = s.position;
    const warnLoc = s.wifiError === 'locationDenied' || s.gpsError === 'denied';
    return `
    ${warnLoc ? `<div class="banner">${icon('info')}<div class="text"><b>Windows bloque l'accès à la position.</b> Active « Services de localisation » et « Autoriser les applications de bureau à accéder à votre position ». Depuis Windows 11 24H2, c'est aussi nécessaire pour lire le nom du Wi-Fi.</div>
      <button class="btn" data-open-settings="location">Ouvrir les paramètres</button></div>` : ''}
    ${s.lastEvent ? `<div class="banner info">${icon('info')}<div class="text">${esc(s.lastEvent)}</div></div>` : ''}

    <div class="grid grid-2">
      <div class="card tight">
        <div class="row">${icon('wifi')}<h3 style="margin:0">Wi-Fi</h3></div>
        <div style="margin-top:10px" class="row wrap">${(s.connected || []).length
          ? s.connected.map(n => `<span class="chip ok">${esc(n)}</span>`).join('')
          : `<span class="faint">${s.wifiError === 'noWifi' ? 'Aucune carte Wi-Fi' : 'Non connecté'}</span>`}</div>
      </div>
      <div class="card tight">
        <div class="row">${icon('gps')}<h3 style="margin:0">Position</h3><span class="spacer"></span>
          <button class="btn ghost" id="pl-locate">${icon('update')} Actualiser</button></div>
        <div style="margin-top:4px" class="muted">${pos
          ? `${pos.lat.toFixed(5)}, ${pos.lon.toFixed(5)} · précision ± ${nf0.format(pos.accuracy)} m · ${fmtAgo(pos.time)}`
          : s.gpsError === 'unavailable' ? 'Position indisponible' : 'Pas encore demandée'}</div>
      </div>
    </div>

    <div class="row" style="margin:26px 0 10px"><h2 style="margin:0">Mes lieux</h2><span class="spacer"></span>
      <button class="btn primary" id="pl-new">${icon('plus')} Nouveau lieu</button></div>
    <div class="list">
      ${rules.length ? rules.map(r => `
        <div class="list-item">
          <div style="color:${r.inside ? 'var(--ok)' : 'var(--text-3)'}">${icon('place')}</div>
          <div class="main">
            <div class="row" style="gap:8px"><b>${esc(r.place || r.ssid || 'Lieu')}</b>${r.inside ? '<span class="chip ok">Sur place</span>' : ''}</div>
            <div class="row wrap" style="gap:6px;margin-top:4px">
              ${r.ssid ? `<span class="chip">${icon('wifi', 'icon xs')} ${esc(r.ssid)}</span>` : ''}
              ${r.useGps ? `<span class="chip">${icon('gps', 'icon xs')} rayon ${r.radius} m</span>` : ''}
              ${(r.paths || []).slice(0, 3).map(p => `<span class="chip" title="${esc(p)}">${icon('file', 'icon xs')} ${esc(fileName(p))}</span>`).join('')}
              ${(r.paths || []).length > 3 ? `<span class="chip">+${r.paths.length - 3}</span>` : ''}
            </div>
          </div>
          <button class="btn ghost" data-test="${r.id}" title="Ouvrir maintenant">${icon('play')} Tester</button>
          <button class="btn ghost icon-only" data-edit="${r.id}" title="Modifier">${icon('edit')}</button>
          <input type="checkbox" class="switch" data-rule-toggle="${r.id}" ${r.enabled ? 'checked' : ''} title="Activer ce lieu">
        </div>`).join('')
      : `<div class="card empty">${icon('place')}<div>Ajoute un lieu : quand tu y arrives, ToolBox ouvre les apps de ton choix.</div></div>`}
    </div>`;
  },
  bind(root) {
    $('[data-open-settings]', root)?.addEventListener('click', e => send({ type: 'openSettingsPage', page: e.currentTarget.dataset.openSettings }));
    $('#pl-locate', root).addEventListener('click', () => { moduleAction('place_launcher', 'locate'); toast('Recherche de la position…'); });
    $('#pl-new', root).addEventListener('click', () => this.edit(null));
    $$('[data-edit]', root).forEach(b => b.addEventListener('click', () => this.edit(b.dataset.edit)));
    $$('[data-test]', root).forEach(b => b.addEventListener('click', () => moduleAction('place_launcher', 'testRule', { id: b.dataset.test })));
    $$('[data-rule-toggle]', root).forEach(b => b.addEventListener('change', () => {
      const r = (modState('place_launcher').rules || []).find(x => x.id === b.dataset.ruleToggle);
      if (r) moduleAction('place_launcher', 'saveRule', { rule: { ...r, enabled: b.checked } });
    }));
  },
  edit(id) {
    const s = modState('place_launcher');
    const existing = (s.rules || []).find(r => r.id === id);
    const r = existing ? { ...existing, paths: [...(existing.paths || [])] } : { id: '', place: '', ssid: (s.connected || [])[0] || '', useGps: false, lat: 0, lon: 0, radius: 150, paths: [], enabled: true };
    const dlg = $('#dialog');
    let results = [];
    const draw = () => {
      const st = modState('place_launcher');
      dlg.innerHTML = `
      <div class="dlg-body">
        <h2>${existing ? 'Modifier le lieu' : 'Nouveau lieu'}</h2>
        <label class="field"><span>Nom du lieu</span><input type="text" id="r-place" value="${esc(r.place)}" placeholder="Ex. Maison"></label>

        <div class="field"><span>Apps et fichiers à ouvrir (${r.paths.length})</span>
          <div class="list">${r.paths.length ? r.paths.map((p, i) => `
            <div class="list-item" style="padding:8px 12px">${icon('file')}
              <div class="main ellipsis" title="${esc(p)}">${esc(fileName(p))}<div class="faint small ellipsis">${esc(p)}</div></div>
              <button class="btn ghost icon-only" data-rm-path="${i}" title="Retirer">${icon('close')}</button></div>`).join('')
            : '<div class="faint small">Aucune app pour l\'instant.</div>'}</div>
          <div><button class="btn" id="r-pick">${icon('plus')} Ajouter une app ou un fichier</button></div></div>

        <div class="setting" style="margin:0;padding:12px 16px">${icon('wifi')}
          <div class="text"><div class="title">Réseau Wi-Fi</div><div class="desc">Laisse vide pour ne pas l'utiliser.</div></div>
          <input type="text" id="r-ssid" value="${esc(r.ssid)}" placeholder="Nom du Wi-Fi" style="width:190px" list="ssids">
          <datalist id="ssids">${(st.connected || []).map(n => `<option value="${esc(n)}">`).join('')}</datalist>
        </div>

        <div class="setting" style="margin:0;padding:12px 16px;flex-wrap:wrap">${icon('gps')}
          <div class="text"><div class="title">Position GPS</div><div class="desc">Utilise le GPS du PC s'il en a un, sinon la position estimée par Windows.</div></div>
          <input type="checkbox" class="switch" id="r-gps" ${r.useGps ? 'checked' : ''}>
          ${r.useGps ? `
          <div style="flex-basis:100%;display:flex;flex-direction:column;gap:10px;margin-top:8px">
            <div class="row"><input type="search" id="r-addr" placeholder="Chercher une adresse (ex. 500 boul. de Mortagne, Boucherville)"><button class="btn" id="r-search">${icon('search')}</button></div>
            <div class="suggestions">${results.map((x, i) => `<button data-res="${i}">${esc(x.display_name)}</button>`).join('')}</div>
            <div class="row wrap"><button class="btn" id="r-here">${icon('gps')} Ma position actuelle</button>
              <span class="muted small">${r.lat || r.lon ? `${(+r.lat).toFixed(5)}, ${(+r.lon).toFixed(5)}` : 'Aucune position choisie'}</span></div>
            <div class="row"><span class="muted" style="width:60px">Rayon</span><input type="range" id="r-radius" min="30" max="2000" step="10" value="${r.radius}"><b id="r-radius-v" style="width:70px;text-align:right">${r.radius} m</b></div>
          </div>` : ''}
        </div>
        <div class="faint small">Au moins un des deux (Wi-Fi ou GPS) doit correspondre pour être « sur place ». L'app s'ouvre à ton arrivée, et aussi au démarrage du PC si tu es déjà sur place.</div>
      </div>
      <div class="dlg-foot">
        ${existing ? `<button class="btn danger left" id="r-del">${icon('trash')} Supprimer</button>` : ''}
        <button class="btn" id="r-cancel">Annuler</button>
        <button class="btn primary" id="r-save">Enregistrer</button>
      </div>`;

      const keep = () => {
        r.place = $('#r-place', dlg).value; r.ssid = $('#r-ssid', dlg).value.trim();
        const rad = $('#r-radius', dlg); if (rad) r.radius = +rad.value;
      };
      $('#r-pick', dlg).addEventListener('click', async () => { keep(); const p = await pickFile(); if (p && !r.paths.includes(p)) { r.paths.push(p); draw(); } });
      $$('[data-rm-path]', dlg).forEach(b => b.addEventListener('click', () => { keep(); r.paths.splice(+b.dataset.rmPath, 1); draw(); }));
      $('#r-gps', dlg).addEventListener('change', e => { keep(); r.useGps = e.target.checked; draw(); });
      const rad = $('#r-radius', dlg);
      if (rad) { setRangeFill(rad); rad.addEventListener('input', () => { $('#r-radius-v', dlg).textContent = rad.value + ' m'; setRangeFill(rad); }); }
      $('#r-here', dlg)?.addEventListener('click', () => {
        keep();
        const p = modState('place_launcher').position;
        if (p) { r.lat = p.lat; r.lon = p.lon; draw(); toast('Position enregistrée'); }
        else { moduleAction('place_launcher', 'locate'); toast('Recherche de la position… réessaie dans quelques secondes'); }
      });
      const search = async () => {
        keep();
        const q = $('#r-addr', dlg).value.trim();
        if (!q) return;
        try {
          const res = await fetch('https://nominatim.openstreetmap.org/search?format=json&limit=5&accept-language=fr&q=' + encodeURIComponent(q));
          results = await res.json();
          if (!results.length) toast('Adresse introuvable');
        } catch { toast('Recherche impossible (pas de connexion ?)'); }
        draw();
      };
      $('#r-search', dlg)?.addEventListener('click', search);
      $('#r-addr', dlg)?.addEventListener('keydown', e => { if (e.key === 'Enter') { e.preventDefault(); search(); } });
      $$('[data-res]', dlg).forEach(b => b.addEventListener('click', () => {
        const x = results[+b.dataset.res];
        keep(); r.lat = +x.lat; r.lon = +x.lon; results = [];
        if (!r.place) r.place = x.display_name.split(',')[0];
        draw();
      }));
      $('#r-cancel', dlg).addEventListener('click', () => dlg.close());
      $('#r-del', dlg)?.addEventListener('click', () => { moduleAction('place_launcher', 'deleteRule', { id: r.id }); dlg.close(); });
      $('#r-save', dlg).addEventListener('click', () => {
        keep();
        if (!r.paths.length) return toast('Ajoute au moins une app ou un fichier');
        if (!r.ssid && !(r.useGps && (r.lat || r.lon))) return toast('Indique un Wi-Fi ou une position GPS');
        moduleAction('place_launcher', 'saveRule', { rule: r });
        dlg.close();
      });
    };
    draw();
    dlg.showModal();
    dlg.addEventListener('close', () => renderPage(true), { once: true });
  },
};

// ------------------------------------------------------------------ Paramètres

const SettingsPage = {
  render() {
    const a = S.app;
    const row = (key, ic, title, desc) => `<div class="setting">${icon(ic)}
      <div class="text"><div class="title">${title}</div><div class="desc">${desc}</div></div>
      <input type="checkbox" class="switch" data-setting="${key}" ${a[key] ? 'checked' : ''}></div>`;
    return `
    ${row('startWithWindows', 'startup', 'Démarrer avec Windows', 'ToolBox se lance en arrière-plan à l\'ouverture de session.')}
    ${row('minimizeToTray', 'tray', 'Réduire dans la barre des tâches', 'Le bouton ✕ cache la fenêtre au lieu de quitter. Clic droit sur l\'icône près de l\'horloge pour quitter.')}
    <div class="section-title">Performance</div>
    ${row('lowMemory', 'leaf', 'Économiser la mémoire', 'Quand ToolBox reste caché 3 minutes, l\'interface est libérée (environ 60 Mo). Elle se recharge en une demi-seconde à la réouverture.')}
    ${row('instantPopup', 'clipboard', 'Fenêtre du presse-papiers instantanée', 'Préparée dès le démarrage pour s\'ouvrir tout de suite (environ 30 Mo de plus).')}
    <div class="section-title">Fonctions</div>
    ${S.order.filter(id => !S.modules[id].alwaysOn).map(id => { const m = S.modules[id]; const def = PAGES.find(p => p.module === id) || {}; return `<div class="setting">${icon(def.icon || 'apps')}
      <div class="text"><div class="title">${esc(m.name)}</div><div class="desc">${esc(m.description)}</div></div>
      <input type="checkbox" class="switch" data-module-switch="${id}" ${m.enabled ? 'checked' : ''}></div>`; }).join('')}
    <div class="section-title">Données</div>
    <div class="setting">${icon('folder')}
      <div class="text"><div class="title">Dossier des réglages</div><div class="desc mono">${esc(a.dataDir)}</div></div>
      <button class="btn" id="open-data">Ouvrir</button></div>`;
  },
  bind(root) {
    $$('[data-setting]', root).forEach(el => el.addEventListener('change', () => send({ type: 'setSetting', key: el.dataset.setting, value: el.checked })));
    $('#open-data', root).addEventListener('click', () => send({ type: 'openDataDir' }));
  },
};

// ------------------------------------------------------------------ Mises à jour

const UpdatesPage = {
  render() {
    const u = S.update || {};
    const msg = {
      idle: ['', 'Pas encore vérifié'],
      checking: ['', 'Vérification…'],
      upToDate: ['ok', 'ToolBox est à jour'],
      available: ['warn', `Version ${u.latest} disponible`],
      downloading: ['warn', `Téléchargement… ${u.progress || 0} %`],
      ready: ['ok', 'Mise à jour installée : redémarre pour l\'utiliser'],
      error: ['warn', u.error || 'Erreur'],
    }[u.status] || ['', ''];
    return `
    <div class="card">
      <div class="row wrap" style="gap:40px">
        <div><div class="label">Version installée</div><div class="value big">${esc(u.current || S.app.version)}</div></div>
        ${u.latest ? `<div><div class="label">Dernière version</div><div class="value big">${esc(u.latest.replace(/^v/, ''))}</div></div>` : ''}
        <span class="spacer"></span>
        <div class="stack" style="align-items:flex-end">
          <span class="pill ${msg[0]}"><span class="led"></span>${esc(msg[1])}</span>
          <span class="faint small">Dernière vérification : ${fmtAgo(u.lastCheck)}</span>
        </div>
      </div>
      <div class="row wrap" style="margin-top:24px">
        ${u.status === 'available' ? `<button class="btn primary big" id="up-install">${icon('update')} Installer ${esc(u.latest)}</button>` : ''}
        ${u.status === 'ready' ? `<button class="btn primary big" id="up-restart">${icon('update')} Redémarrer ToolBox</button>` : ''}
        <button class="btn big" id="up-check" ${['checking', 'downloading'].includes(u.status) ? 'disabled' : ''}>Rechercher des mises à jour</button>
      </div>
    </div>
    <div class="setting">${icon('update')}
      <div class="text"><div class="title">Mises à jour automatiques</div>
        <div class="desc">Vérifie toutes les 6 heures, installe en arrière-plan et redémarre ToolBox quand la fenêtre est fermée.</div></div>
      <input type="checkbox" class="switch" id="up-auto" ${S.app.autoUpdate ? 'checked' : ''}></div>
    ${u.notes ? `<div class="section-title">Nouveautés</div><div class="card tight" style="white-space:pre-wrap;user-select:text">${esc(u.notes)}</div>` : ''}`;
  },
  bind(root) {
    $('#up-check', root).addEventListener('click', () => send({ type: 'checkUpdate' }));
    $('#up-install', root)?.addEventListener('click', () => send({ type: 'installUpdate' }));
    $('#up-restart', root)?.addEventListener('click', () => send({ type: 'restart' }));
    $('#up-auto', root).addEventListener('change', e => send({ type: 'setSetting', key: 'autoUpdate', value: e.target.checked }));
  },
};

// ------------------------------------------------------------------ Mode aperçu (navigateur, sans le C++)

const Mock = {
  state: null,
  stats(now) {
    const days = [];
    for (let i = 6; i >= 0; i--) {
      const d = new Date(); d.setDate(d.getDate() - i);
      const date = `${d.getFullYear()}-${String(d.getMonth() + 1).padStart(2, '0')}-${String(d.getDate()).padStart(2, '0')}`;
      days.push({ date, active: 3600 + ((i * 2477) % 14000), events: { copy: 12 + i, ocr: 2, screenshot: 1, enterFix: 3 },
        apps: [['Google Chrome', 5400], ['rekordbox', 2600], ['Minecraft', 2100], ['Visual Studio Code', 1500], ['Discord', 900], ['LumaFusion', 400]].map(([name, sec]) => ({ name, sec: sec - i * 60 })) });
    }
    const h0 = Math.floor(now / 3600) - 47;
    return { days, week: days[6].apps.map(a => ({ ...a, sec: a.sec * 6 })), since: now - 86400 * 9,
      hours: Array.from({ length: 48 }, (_, i) => ({ hour: h0 + i, cpu: 15 + 20 * Math.abs(Math.sin(i / 5)), ram: 55 + 10 * Math.sin(i / 9) })),
      totals: { copy: 320, ocr: 41, screenshot: 17, enterFix: 58, appGroup: 23, placeLaunch: 9, programStop: 14, programRelaunch: 11 } };
  },
  init() {
    const now = Math.floor(Date.now() / 1000);
    this.state = {
      type: 'state',
      app: { version: '0.4.0', userName: 'Médéric', enabled: true, lowMemory: true, instantPopup: true, startWithWindows: true, minimizeToTray: true, autoUpdate: true, dataDir: 'C:\\Users\\Mederic\\AppData\\Roaming\\ToolBox' },
      update: { current: '0.1.0', status: 'upToDate', latest: 'v0.1.0', notes: '', progress: 0, lastCheck: now - 120 },
      modules: [
        { id: 'monitor', name: 'Moniteur', description: 'Processeur, mémoire, disque et batterie en direct.', enabled: true, alwaysOn: true, running: true, state: {} },
        { id: 'converter', name: 'Convertisseur', description: 'Unités et devises.', enabled: true, alwaysOn: true, running: true, state: {} },
        { id: 'stats', name: 'Statistiques', description: "Temps d'écran par app, utilisation du PC et tes chiffres ToolBox (tout reste sur ton PC).", enabled: true, running: true, state: Mock.stats(now) },
        { id: 'processes', name: 'Programmes', description: "Arrête les programmes inutiles pour libérer de la mémoire, puis relance-les d'un clic.", enabled: true, running: true, state: {
          useless: ['c:\\program files\\teams\\ms-teams.exe'], lastEvent: '', stopped: [{ path: 'C:\\Users\\Mederic\\AppData\\Local\\Discord\\Discord.exe', name: 'Discord', time: now - 300 }] } },
        { id: 'enter_guard', name: 'Garde Enter', description: "Supprime la touche voisine d'Enter frappée par accident.", enabled: true, running: true, state: { keyName: 'À', isDefaultKey: true, thresholdMs: 80, corrections: 12, exclusions: [], capturing: false } },
        { id: 'clipboard', name: 'Presse-papiers', description: 'Retrouve tout ce que tu as copié.', enabled: true, running: true, state: { maxItems: 50, keepAfterRestart: false, hotkeyEnabled: true, hotkeyLabel: 'Ctrl + Alt + V', hotkeyError: '', autoPaste: true, items: [
          { id: 3, text: 'https://github.com/Mede2026/ToolBox', time: now - 30, pinned: false },
          { id: 2, text: 'Exercice 4 : 3/4 + 5/6 = 19/12', time: now - 600, pinned: true },
          { id: 1, text: 'Massif de Charlevoix — horaire des remontées', time: now - 7200, pinned: false }] } },
        { id: 'ocr', name: "Capture d'écran", description: "Capture une zone de l'écran en image, ou lis le texte qu'elle contient (OCR).", enabled: true, running: true, state: {
          hotkeyEnabled: true, hotkeyLabel: 'Ctrl + Alt + T', hotkeyError: '', shot_hotkeyEnabled: true, shot_hotkeyLabel: 'Ctrl + Alt + S', shot_hotkeyError: '',
          shotSave: true, shotFolder: 'C:\\Users\\Mederic\\Pictures\\Captures ToolBox',
          shots: [{ id: 9, path: 'C:\\Users\\Mederic\\Pictures\\Captures ToolBox\\Capture 2026-09-29 23h40 12.png', time: now - 200, w: 1280, h: 720, exists: true }], singleLine: false, notify: true, status: 'idle', detail: '', engineLoaded: false,
          langs: [['fra', 'Français', 1130365, true, true], ['eng', 'Anglais', 4113088, true, true], ['spa', 'Espagnol', 2294433, false, false], ['deu', 'Allemand', 1525436, false, false], ['ita', 'Italien', 2701314, false, false], ['por', 'Portugais', 1982756, false, false]]
            .map(([code, name, size, installed, selected]) => ({ code, name, size, installed, selected })),
          history: [{ id: 1, text: 'Chapitre 3 — Les fractions équivalentes\nDeux fractions sont équivalentes si elles représentent la même quantité.', time: now - 90, ms: 640 }] } },
        { id: 'color', name: 'Pipette', description: "Copie la couleur de n'importe quel pixel de l'écran (HEX, RGB ou HSL).", enabled: true, running: true, state: {
          hotkeyEnabled: true, hotkeyLabel: 'Ctrl + Alt + C', hotkeyError: '', format: 'hex',
          history: [['#7CBCFF', 'rgb(124, 188, 255)', 'hsl(211, 100%, 74%)'], ['#1ED760', 'rgb(30, 215, 96)', 'hsl(141, 76%, 48%)'], ['#FF5A36', 'rgb(255, 90, 54)', 'hsl(11, 100%, 61%)']]
            .map(([hex, rgb, hsl], i) => ({ id: 20 + i, hex, rgb, hsl, time: now - i * 300 })) } },
        { id: 'app_launcher', name: "Raccourcis d'apps", description: 'Lance plusieurs apps d\'un seul clic.', enabled: true, running: true, state: { groups: [
          { id: 'g1', name: 'Devoirs', emoji: '📚', items: ['C:\\Program Files\\Microsoft Office\\WINWORD.EXE', 'https://www.alloprof.qc.ca'] },
          { id: 'g2', name: 'DJ', emoji: '🎧', items: ['C:\\Program Files\\rekordbox\\rekordbox.exe'] }] } },
        { id: 'place_launcher', name: 'Lancement par lieu', description: 'Ouvre un fichier ou une app quand tu arrives à un endroit (Wi-Fi ou GPS).', enabled: true, running: true, state: {
          connected: ['Maison-5G'], wifiError: '', gpsError: '', lastEvent: '', position: { lat: 45.5913, lon: -73.4364, accuracy: 35, time: now - 40 },
          rules: [{ id: 'r1', place: 'Maison', ssid: 'Maison-5G', useGps: true, lat: 45.5913, lon: -73.4364, radius: 150, paths: ['C:\\Program Files\\rekordbox\\rekordbox.exe', 'C:\\Program Files\\Spotify\\Spotify.exe'], enabled: true, inside: true }] } },
      ],
    };
    setTimeout(() => receive(this.state), 50);
    let t = 0;
    setInterval(() => {
      t++;
      receive({ type: 'live', live: { monitor: {
        cpu: 35 + 20 * Math.sin(t / 5) + Math.random() * 10, cores: 16,
        ram: { used: 9.1e9 + Math.random() * 3e8, total: 16e9 }, disk: { drive: 'C:', used: 312e9, total: 512e9 },
        battery: { percent: 56, charging: false, secondsLeft: 9800, saver: true },
        net: { down: 1.4e6 + Math.random() * 5e5, up: 2e5 }, uptime: 18000 + t },
        processes: { programs: [
          { key: 'c:\\program files\\google\\chrome\\application\\chrome.exe', path: 'C:\\Program Files\\Google\\Chrome\\Application\\chrome.exe', name: 'Google Chrome', exe: 'chrome.exe', count: 14, ram: 2.1e9, cpu: 4.2 + Math.random(), windowed: true, title: 'YouTube - Google Chrome', useless: false },
          { key: 'c:\\program files\\teams\\ms-teams.exe', path: 'C:\\Program Files\\Teams\\ms-teams.exe', name: 'Microsoft Teams', exe: 'ms-teams.exe', count: 5, ram: 6.4e8, cpu: 0.8, windowed: false, title: '', useless: true },
          { key: 'c:\\program files\\rekordbox\\rekordbox.exe', path: 'C:\\Program Files\\rekordbox\\rekordbox.exe', name: 'rekordbox', exe: 'rekordbox.exe', count: 1, ram: 1.2e9, cpu: 12.5, windowed: true, title: 'rekordbox', useless: false }] } } });
    }, 1000);
  },
  handle(msg) {
    const s = this.state;
    const mod = id => s.modules.find(m => m.id === id);
    if (msg.type === 'setModule') mod(msg.id).enabled = msg.enabled;
    else if (msg.type === 'setGlobal') s.app.enabled = msg.enabled;
    else if (msg.type === 'setSetting') s.app[msg.key] = msg.value;
    else if (msg.type === 'pickFile') return setTimeout(() => receive({ type: 'filePicked', requestId: msg.requestId, path: 'C:\\Program Files\\Exemple\\app.exe' }), 100);
    else if (msg.type === 'moduleAction' && msg.id === 'converter' && msg.action === 'setPref') mod('converter').state[msg.payload.key] = msg.payload.value;
    else if (msg.type === 'moduleAction' && msg.action === 'setHotkey') mod(msg.id).state[msg.payload.hotkey ? msg.payload.hotkey + '_hotkeyLabel' : 'hotkeyLabel'] = msg.payload.label;
    else if (msg.type === 'moduleAction' && msg.id === 'color' && msg.action === 'setFormat') mod('color').state.format = msg.payload.value;
    else if (msg.type === 'moduleAction' && msg.id === 'ocr' && msg.action === 'setLangs') mod('ocr').state.langs.forEach(l => { l.selected = msg.payload.list.includes(l.code); });
    else return;
    receive(JSON.parse(JSON.stringify(s)));
  },
};

// ------------------------------------------------------------------ Démarrage

if (window.TOOLBOX_POPUP) document.body.classList.add('popup');
if (webview) {
  webview.addEventListener('message', e => receive(e.data));
  send({ type: 'ready' });
} else {
  Mock.init();
}
renderNav();
