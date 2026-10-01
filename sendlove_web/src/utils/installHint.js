/**
 * Trạng thái "cài app lên màn hình chính" (PWA) của trình duyệt hiện tại.
 */

const DISMISS_KEY = 'sendlove-install-hint';

/** Trình duyệt nhúng trong app khác: Google chặn đăng nhập OAuth ở các WebView này. */
const IN_APP_BROWSER = /FBAN|FBAV|FB_IAB|Instagram|Zalo|Line\//i;

/**
 * Hàm thuần — nhận userAgent thay vì tự đọc navigator để thử được không cần thiết bị.
 * @returns {'installed' | 'in-app' | 'ios' | 'other'}
 *   installed: đang chạy từ icon màn hình chính, không cần gợi ý gì.
 *   in-app:    trình duyệt trong Zalo/Facebook/… — phải mở bằng trình duyệt thật.
 *   ios:       iPhone/iPad — không có lời mời cài tự động, phải hướng dẫn bấm Chia sẻ.
 *   other:     Android/desktop — chờ sự kiện beforeinstallprompt của trình duyệt.
 */
export function detectInstallMode({ userAgent, standalone, maxTouchPoints = 0 }) {
  if (standalone) return 'installed';
  if (IN_APP_BROWSER.test(userAgent)) return 'in-app';
  // iPadOS tự xưng là Macintosh; phân biệt với máy Mac thật bằng màn cảm ứng.
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
  try { localStorage.setItem(DISMISS_KEY, 'off'); } catch { /* trình duyệt chặn storage, bỏ qua */ }
}

/*
 * beforeinstallprompt (Chrome/Edge trên Android và desktop) bắn MỘT lần, sớm,
 * có thể trước khi màn Login kịp dựng — nên phải nghe ngay khi module được nạp
 * và giữ sự kiện lại cho component lấy sau.
 */
let installPrompt = null;
const listeners = new Set();

function setInstallPrompt(event) {
  installPrompt = event;
  listeners.forEach((notify) => notify());
}

window.addEventListener('beforeinstallprompt', (event) => {
  event.preventDefault(); // tự hiện nút của mình thay cho thanh mời mặc định
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

/** Mở hộp thoại cài đặt của trình duyệt. Sự kiện chỉ dùng được một lần. */
export async function promptInstall() {
  const event = installPrompt;
  if (!event) return;
  setInstallPrompt(null);
  await event.prompt();
}
