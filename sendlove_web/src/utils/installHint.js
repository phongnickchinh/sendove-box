/**
 * "Add to home screen" (PWA) state of the current browser.
 */

const DISMISS_KEY = 'sendlove-install-hint';

/** Browsers embedded in other apps: Google blocks OAuth sign-in in these WebViews. */
const IN_APP_BROWSER = /FBAN|FBAV|FB_IAB|Instagram|Zalo|Line\//i;

/**
 * Pure — takes the userAgent instead of reading navigator so it can be tested without a device.
 * @returns {'installed' | 'in-app' | 'ios' | 'other'}
 *   installed: running from the home-screen icon; no hint needed.
 *   in-app:    browser inside Zalo/Facebook/… — must be reopened in a real browser.
 *   ios:       iPhone/iPad — no automatic install prompt; point the user at Share.
 *   other:     Android/desktop — wait for the browser's beforeinstallprompt event.
 */
export function detectInstallMode({ userAgent, standalone, maxTouchPoints = 0 }) {
  if (standalone) return 'installed';
  if (IN_APP_BROWSER.test(userAgent)) return 'in-app';
  // iPadOS reports itself as Macintosh; a touch screen tells it apart from a real Mac.
  const isIOS = /iPhone|iPad|iPod/.test(userAgent)
    || (/Macintosh/.test(userAgent) && maxTouchPoints > 1);
  return isIOS ? 'ios' : 'other';
}

export function currentInstallMode() {
  const standalone = window.navigator.standalone === true
    || window.matchMedia('(display-mode: standalone)').matches;
  return detectInstallMode({
    userAgent: window.navigator.userAgent,
    standalone,
    maxTouchPoints: window.navigator.maxTouchPoints,
  });
}

export function isHintDismissed() {
  try { return localStorage.getItem(DISMISS_KEY) === 'off'; } catch { return false; }
}

export function dismissHint() {
  try { localStorage.setItem(DISMISS_KEY, 'off'); } catch { /* storage blocked by the browser; ignore */ }
}

/*
 * beforeinstallprompt (Chrome/Edge on Android and desktop) fires ONCE, early,
 * possibly before the Login screen mounts — so listen at module load and keep
 * the event for the component to pick up later.
 */
let installPrompt = null;
const listeners = new Set();

function setInstallPrompt(event) {
  installPrompt = event;
  listeners.forEach((notify) => notify());
}

window.addEventListener('beforeinstallprompt', (event) => {
  event.preventDefault(); // show our own button instead of the default install banner
  setInstallPrompt(event);
});
window.addEventListener('appinstalled', () => setInstallPrompt(null));

export function subscribeInstallPrompt(notify) {
  listeners.add(notify);
  return () => listeners.delete(notify);
}

export function getInstallPrompt() {
  return installPrompt;
}

/** Open the browser's install dialog. The event can be used only once. */
export async function promptInstall() {
  const event = installPrompt;
  if (!event) return;
  setInstallPrompt(null);
  await event.prompt();
}
