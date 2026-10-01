/*
 * Sendlove Box's minimal service worker.
 *
 * It INTENTIONALLY caches no JS/CSS/API: everything still comes from the
 * network as if there were no service worker, so a new deploy reaches users on
 * the next load — no stale versions. Its only job: when a PAGE LOAD fails
 * offline, serve offline.html (cached at install) instead of the browser's
 * error page.
 */
// Bump this version whenever offline.html changes, or installed devices keep the old copy forever.
const CACHE = 'sendlove-offline-v1';
const OFFLINE_URL = '/offline.html';

self.addEventListener('install', (event) => {
  event.waitUntil(
    caches.open(CACHE).then((cache) => cache.add(new Request(OFFLINE_URL, { cache: 'reload' }))),
  );
  self.skipWaiting();
});

self.addEventListener('activate', (event) => {
  event.waitUntil(
    caches.keys()
      .then((keys) => Promise.all(keys.filter((key) => key !== CACHE).map((key) => caches.delete(key))))
      .then(() => self.clients.claim()),
  );
});

self.addEventListener('fetch', (event) => {
  const { request } = event;
  if (request.mode !== 'navigate' || request.method !== 'GET') return;

  const url = new URL(request.url);
  // /__/ is Firebase Hosting's redirect sign-in handler — never intercept it.
  if (url.origin !== self.location.origin || url.pathname.startsWith('/__/')) return;

  event.respondWith(fetch(request).catch(() => caches.match(OFFLINE_URL)));
});
