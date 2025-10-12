(function(){
  const el = (id) => document.getElementById(id);
  const $ = (sel, root=document) => root.querySelector(sel);
  const $$ = (sel, root=document) => Array.from(root.querySelectorAll(sel));

  const state = {
    cfg: null,
  };

  function detectOS(){
    const ua = navigator.userAgent || navigator.vendor || window.opera;
    const platform = navigator.platform || '';
    const isWindows = /Win/.test(platform) || /Windows/i.test(ua);
    const isMac = /Mac/.test(platform) || /Macintosh|Mac OS X/i.test(ua);
    const isLinux = /Linux/.test(platform) && !/Android/i.test(ua);
    const isAndroid = /Android/i.test(ua);
    if(isAndroid) return 'android';
    if(isWindows) return 'windows';
    if(isMac) return 'macos';
    if(isLinux) return 'linux';
    return 'unknown';
  }

  async function loadConfig(){
    try{
      const res = await fetch('./config.json', { cache: 'no-store' });
      if(!res.ok) throw new Error('config load failed');
      const cfg = await res.json();
      state.cfg = cfg;
      applyConfig(cfg);
    }catch(e){
      console.warn('Failed to load config.json; using defaults', e);
      const cfg = defaultConfig();
      state.cfg = cfg;
      applyConfig(cfg);
    }
  }

  function defaultConfig(){
    return {
      name: 'Almuslim',
      tagline: 'Prayer times and Hijri calendar for desktop.',
      repo: '#',
      downloads: {
        windows: { url: '#', label: 'Almuslim-Setup.exe', checksum: '' },
        linux_appimage: { url: '#', label: 'Almuslim-x86_64.AppImage', checksum: '' },
        linux_deb: { url: '#', label: 'almuslim_amd64.deb', checksum: '' },
        macos: { url: '#', label: 'Almuslim.dmg', checksum: '' },
        android: { url: '#', label: 'Almuslim.apk', checksum: '' }
      }
    };
  }

  function applyConfig(cfg){
    // Branding
    el('brand-name').textContent = cfg.name || 'Almuslim';
    el('app-title').textContent = cfg.name || 'Almuslim';
    if(cfg.tagline) el('app-tagline').textContent = cfg.tagline;
    const year = new Date().getFullYear();
    el('year').textContent = String(year);
    el('footer-name').textContent = cfg.name || 'Almuslim';
    const repoLink = el('repo-link');
    if(repoLink && cfg.repo){ repoLink.href = cfg.repo; }

    // Downloads
    const setLink = (id, item) => {
      const a = el(id);
      if(!a) return;
      if(item && item.url && item.url !== '#'){
        a.href = item.url;
        a.textContent = a.textContent.replace('Download', `Download (${item.label||'latest'})`);
      }else{
        a.classList.add('disabled');
        a.href = '#downloads';
        a.textContent = 'Coming soon';
      }
    };

    setLink('dl-windows', cfg.downloads?.windows);
    setLink('dl-linux-appimage', cfg.downloads?.linux_appimage);
    setLink('dl-linux-deb', cfg.downloads?.linux_deb);
    setLink('dl-macos', cfg.downloads?.macos);
    setLink('dl-android', cfg.downloads?.android);

    // Primary CTA OS targeting
    const os = detectOS();
    const primary = el('primary-download');
    const osLabel = $('.os-label', primary);
    let target, hintEl;
    switch(os){
      case 'windows': target = cfg.downloads?.windows; hintEl = el('hint-windows'); break;
      case 'linux': target = cfg.downloads?.linux_appimage || cfg.downloads?.linux_deb; hintEl = el('hint-linux'); break;
      case 'macos': target = cfg.downloads?.macos; hintEl = el('hint-macos'); break;
      case 'android': target = cfg.downloads?.android; hintEl = el('hint-android'); break;
      default: target = null;
    }
    if(target && target.url && target.url !== '#'){
      primary.href = target.url;
      if(osLabel) osLabel.textContent = `Download for ${os.charAt(0).toUpperCase()+os.slice(1)}`;
      if(hintEl && target.label){ hintEl.textContent = `File: ${target.label}`; }
    }else{
      primary.href = '#downloads';
      if(osLabel) osLabel.textContent = 'View all downloads';
    }

    // Checksums
    const list = el('checksums-list');
    if(list){
      list.innerHTML = '';
      const items = [
        ['Windows', cfg.downloads?.windows],
        ['Linux AppImage', cfg.downloads?.linux_appimage],
        ['Linux .deb', cfg.downloads?.linux_deb],
        ['macOS', cfg.downloads?.macos],
        ['Android', cfg.downloads?.android],
      ];
      for(const [name, item] of items){
        if(!item || (!item.checksum && !item.label)) continue;
        const li = document.createElement('li');
        li.innerHTML = `<strong>${name}</strong>: ${item.label ? `<code>${item.label}</code>` : ''} ${item.checksum ? `— SHA256 <code>${item.checksum}</code>` : ''}`;
        list.appendChild(li);
      }
      if(!list.children.length){
        const li = document.createElement('li');
        li.className = 'muted';
        li.textContent = 'No checksums provided.';
        list.appendChild(li);
      }
    }
  }

  // Init
  window.addEventListener('DOMContentLoaded', loadConfig);
})();
