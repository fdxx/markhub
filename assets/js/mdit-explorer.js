(() => {
  const loaded = Symbol.for('mdit-explorer.loaded');
  if (window[loaded]) return;
  window[loaded] = true;

  const select = (root, button) => {
    const path = button.dataset.path;
    root.querySelectorAll('.mexp__file').forEach(element => {
      element.setAttribute('aria-selected', String(element === button));
    });
    root.querySelectorAll('.mexp__view').forEach(element => {
      element.hidden = element.dataset.path !== path;
    });
    root.querySelector('.mexp__tab-name').textContent = button.dataset.name;
    root.querySelector('.mexp__crumb').textContent = path;
    root.querySelector('.mexp__copy').dataset.path = path;
  };

  const copyText = async text => {
    if (navigator.clipboard?.writeText) {
      await navigator.clipboard.writeText(text);
      return;
    }
    const area = document.createElement('textarea');
    area.value = text;
    area.style.cssText = 'position:fixed;opacity:0';
    document.body.append(area);
    area.select();
    const copied = document.execCommand('copy');
    area.remove();
    if (!copied) throw new Error('Copy failed');
  };

  document.addEventListener('click', async event => {
    const file = event.target.closest('.mexp__file');
    if (file) {
      select(file.closest('.mexp'), file);
      return;
    }

    const copy = event.target.closest('.mexp__copy');
    if (!copy) return;
    const root = copy.closest('.mexp');
    const view = [...root.querySelectorAll('.mexp__view')]
      .find(element => element.dataset.path === copy.dataset.path);
    if (!view) return;

    try {
      await copyText(view.textContent);
      copy.setAttribute('aria-label', 'Copied');
      setTimeout(() => copy.setAttribute('aria-label', 'Copy code'), 1200);
    } catch {
      copy.setAttribute('aria-label', 'Copy failed');
    }
  });
})();
