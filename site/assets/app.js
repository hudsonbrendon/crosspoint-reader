// Informational device picker: the same firmware flashes to both X3 and X4
// (runtime auto-detection), so this only updates the helper note.
document.querySelectorAll('.device-pick button').forEach((btn) => {
  btn.addEventListener('click', () => {
    document.querySelectorAll('.device-pick button').forEach((b) => b.classList.remove('active'));
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
