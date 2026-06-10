// InkPoint site interactions. Keep it tiny and dependency-free.
(function () {
  'use strict';

  const reduceMotion =
    window.matchMedia && window.matchMedia('(prefers-reduced-motion: reduce)').matches;

  /* ---- e-ink refresh flash: a quick full-screen invert, like a panel redraw ---- */
  function einkFlash() {
    if (reduceMotion) return;
    const el = document.getElementById('eink-flash');
    if (!el) return;
    document.body.classList.remove('flashing');
    // force reflow so the animation can restart on repeated calls
    void el.offsetWidth;
    document.body.classList.add('flashing');
    window.setTimeout(() => document.body.classList.remove('flashing'), 700);
  }

  // Flash once on first paint (the page "redraws" like the reader does).
  window.requestAnimationFrame(einkFlash);

  // Flash again when the user commits to a primary action.
  document.querySelectorAll('.btn:not(.ghost)').forEach((btn) => {
    btn.addEventListener('click', einkFlash);
  });

  /* ---- informational device picker (same firmware for X3 and X4) ---- */
  const pickButtons = document.querySelectorAll('.device-pick button');
  pickButtons.forEach((btn) => {
    btn.addEventListener('click', () => {
      pickButtons.forEach((b) => b.classList.remove('active'));
      btn.classList.add('active');
      const note = document.getElementById('device-note');
      if (note) {
        const dev = btn.getAttribute('data-device');
        note.textContent =
          dev === 'X3'
            ? 'X3 detected automatically at boot (528×792 panel). The same firmware is flashed.'
            : 'X4 detected automatically at boot (800×480 panel). The same firmware is flashed.';
      }
    });
  });

  /* ---- scroll-triggered reveals (progressive enhancement) ----
     Only arm hiding when IO is supported AND motion is allowed; otherwise the
     CSS leaves [data-io] fully visible, so content is never stuck hidden. */
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
    // Safety net: never leave content hidden if something goes wrong.
    window.setTimeout(() => ioTargets.forEach((el) => el.classList.add('seen')), 2500);
  }

  /* ---- pull the live version from the flasher manifest, if present ---- */
  fetch('firmware/manifest.json', { cache: 'no-store' })
    .then((r) => (r.ok ? r.json() : null))
    .then((m) => {
      if (!m || !m.version) return;
      const tag = 'v' + m.version;
      const meta = document.getElementById('ver-meta');
      if (meta) meta.textContent = 'Latest: ' + tag;
      const brandNav = document.querySelector('.topbar nav');
      if (brandNav && !document.querySelector('.ver-chip')) {
        const chip = document.createElement('span');
        chip.className = 'ver-chip';
        chip.textContent = tag;
        brandNav.insertBefore(chip, brandNav.firstChild);
      }
    })
    .catch(() => {
      /* manifest only exists after a CI deploy; ignore locally */
    });
})();
