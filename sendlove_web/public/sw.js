/*
 * Service worker tối giản của Sendlove Box.
 *
 * CỐ Ý không cache JS/CSS/API: mọi thứ vẫn lấy từ mạng như khi chưa có service
 * worker, nên bản deploy mới luôn tới tay người dùng ngay lần tải sau — không
 * có chuyện kẹt bản cũ. Việc duy nhất nó làm: khi MỞ TRANG mà mất mạng thì trả
 * trang offline.html (đã lưu lúc cài) thay cho màn lỗi của trình duyệt.
 */
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
  // /__/ là trang xử lý đăng nhập redirect của Firebase Hosting — không được đụng.
  if (url.origin !== self.location.origin || url.pathname.startsWith('/__/')) return;

  event.respondWith(fetch(request).catch(() => caches.match(OFFLINE_URL)));
});
