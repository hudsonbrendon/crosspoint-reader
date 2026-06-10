// InkPoint site interactions: e-ink flash, theme, i18n, reveals, version chip.
(function () {
  'use strict';

  const reduceMotion =
    window.matchMedia && window.matchMedia('(prefers-reduced-motion: reduce)').matches;

  const I18N = window.INKPOINT_I18N || { en: {} };
  const LANGS = window.INKPOINT_LANGS || [{ code: 'en', native: 'English', lang: 'en' }];
  const BASE = I18N.en || {};

  /* ---------- e-ink refresh flash ---------- */
  function einkFlash() {
    if (reduceMotion) return;
    const el = document.getElementById('eink-flash');
    if (!el) return;
    document.body.classList.remove('flashing');
    void el.offsetWidth; // restart animation
    document.body.classList.add('flashing');
    window.setTimeout(() => document.body.classList.remove('flashing'), 700);
  }
  window.requestAnimationFrame(einkFlash);

  /* ---------- theme ---------- */
  function applyTheme(theme) {
    document.documentElement.dataset.theme = theme;
    try { localStorage.setItem('ink-theme', theme); } catch (e) {}
    const t = document.querySelector('.theme-toggle');
    if (t) t.setAttribute('aria-label', theme === 'dark' ? 'Switch to light theme' : 'Switch to dark theme');
  }
  document.querySelectorAll('.theme-toggle').forEach((btn) => {
    btn.addEventListener('click', () => {
      const next = document.documentElement.dataset.theme === 'dark' ? 'light' : 'dark';
      applyTheme(next);
      einkFlash();
    });
  });

  /* ---------- i18n ---------- */
  let activeDevice = 'X4';

  function langMeta(code) {
    return LANGS.find((l) => l.code === code) || LANGS[0];
  }
  function hasLang(code) {
    return LANGS.some((l) => l.code === code);
  }
  function detectLang() {
    try {
      const saved = localStorage.getItem('ink-lang');
      if (saved && hasLang(saved)) return saved;
    } catch (e) {}
    const nav = (navigator.language || 'en').toLowerCase();
    const prefix = nav.split('-')[0];
    const hit = LANGS.find(
      (l) => l.lang.toLowerCase() === nav || l.lang.toLowerCase().split('-')[0] === prefix || l.code === prefix
    );
    return hit ? hit.code : 'en';
  }
  function t(dict, key) {
    return dict[key] != null ? dict[key] : BASE[key];
  }
  function updateDeviceNote(dict) {
    const note = document.getElementById('device-note');
    if (!note) return;
    const key = activeDevice === 'X3' ? 'inst_note_x3' : 'inst_note_x4';
    note.textContent = t(dict, key) || '';
  }
  function applyLang(code) {
    const dict = I18N[code] || BASE;
    document.querySelectorAll('[data-i18n]').forEach((el) => {
      const v = t(dict, el.getAttribute('data-i18n'));
      if (v != null) el.textContent = v;
    });
    document.querySelectorAll('[data-i18n-html]').forEach((el) => {
      const v = t(dict, el.getAttribute('data-i18n-html'));
      if (v != null) el.innerHTML = v;
    });
    const meta = langMeta(code);
    document.documentElement.lang = meta.lang;
    document.documentElement.dir = meta.rtl ? 'rtl' : 'ltr';
    updateDeviceNote(dict);
    return dict;
  }

  // Populate the language picker(s).
  const currentLang = detectLang();
  document.querySelectorAll('.lang-select').forEach((sel) => {
    LANGS.forEach((l) => {
      const opt = document.createElement('option');
      opt.value = l.code;
      opt.textContent = l.native;
      sel.appendChild(opt);
    });
    sel.value = currentLang;
    sel.addEventListener('change', () => {
      const code = sel.value;
      try { localStorage.setItem('ink-lang', code); } catch (e) {}
      document.querySelectorAll('.lang-select').forEach((s) => (s.value = code));
      applyLang(code);
      einkFlash();
    });
  });
  let activeDict = applyLang(currentLang);

  /* ---------- device picker (translation-aware) ---------- */
  const pickButtons = document.querySelectorAll('.device-pick button');
  pickButtons.forEach((btn) => {
    btn.addEventListener('click', () => {
      pickButtons.forEach((b) => b.classList.remove('active'));
      btn.classList.add('active');
      activeDevice = btn.getAttribute('data-device') || 'X4';
      updateDeviceNote(activeDict);
    });
  });

  /* ---------- scroll reveals (progressive enhancement) ---------- */
  const ioTargets = document.querySelectorAll('[data-io]');
  if (ioTargets.length && 'IntersectionObserver' in window && !reduceMotion) {
    document.documentElement.classList.add('has-io');
    const io = new IntersectionObserver(
      (entries, obs) => {
        entries.forEach((e) => {
          if (e.isIntersecting) {
            e.target.classList.add('seen');
            obs.unobserve(e.target);
          }
        });
      },
      { rootMargin: '0px 0px -10% 0px', threshold: 0.12 }
    );
    ioTargets.forEach((el) => io.observe(el));
    window.setTimeout(() => ioTargets.forEach((el) => el.classList.add('seen')), 2500);
  }

  /* ---------- live version from the flasher manifest ---------- */
  fetch('firmware/manifest.json', { cache: 'no-store' })
    .then((r) => (r.ok ? r.json() : null))
    .then((m) => {
      if (!m || !m.version) return;
      const tag = 'v' + m.version;
      const meta = document.getElementById('ver-meta');
      if (meta) meta.textContent = (t(activeDict, 'hero_meta3') || 'Latest release') + ': ' + tag;
      document.querySelectorAll('.ver-chip').forEach((c) => (c.textContent = tag));
    })
    .catch(() => {});
})();
