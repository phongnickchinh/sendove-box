/**
 * Định dạng tin nhắn dùng chung cho lịch sử người gửi, danh sách người nhận và
 * popup chi tiết — trước đây mỗi trang tự chép một bản timeAgo/nhãn riêng.
 */

const MINUTE = 60 * 1000;

export function timeAgo(ts) {
  const diff = Date.now() - ts;
  if (diff < MINUTE) return 'vừa xong';
  const mins = Math.floor(diff / MINUTE);
  if (mins < 60) return `${mins} phút trước`;
  const hours = Math.floor(mins / 60);
  if (hours < 24) return `${hours} giờ trước`;
  return `${Math.floor(hours / 24)} ngày trước`;
}

export const clock = (ts) => new Date(ts).toLocaleTimeString('vi-VN', { hour: '2-digit', minute: '2-digit' });

/** "Thứ Hai, 22/09/2026 · 21:47" */
export const fullDate = (ts) => {
  const d = new Date(ts);
  const day = d.toLocaleDateString('vi-VN', { weekday: 'long', day: '2-digit', month: '2-digit', year: 'numeric' });
  return `${day.charAt(0).toUpperCase()}${day.slice(1)} · ${clock(ts)}`;
};

/** Nhãn ngày: hôm nay / hôm qua / ngày tháng. */
export function dayLabel(ts) {
  const d = new Date(ts);
  const today = new Date();
  const yesterday = new Date(today);
  yesterday.setDate(today.getDate() - 1);
  const same = (a, b) => a.toDateString() === b.toDateString();
  if (same(d, today)) return 'Hôm nay';
  if (same(d, yesterday)) return 'Hôm qua';
  return d.toLocaleDateString('vi-VN', { day: '2-digit', month: '2-digit', year: 'numeric' });
}

/**
 * "Tin nhắn tĩnh" được gửi dưới type 'image' (backend không có enum riêng —
 * SenderUI processAndUpload). Nhận ra nó vì nó không bao giờ có ảnh gốc
 * (image_url): chỉ có thumbnail/bin nếu kèm ảnh, bg_music nếu kèm nhạc.
 */
export const isStatic = (msg) => msg.type === 'image' && !msg.image_url;

/** Khoá bộ lọc của một tin — dùng cho chip lọc ở lịch sử. */
export function kindOf(msg) {
  if (isStatic(msg)) return 'static';
  if (msg.type === 'gif') return 'image';
  return msg.type;
}

export const ICON_OF = { video: 'video', image: 'image', gif: 'image', voice: 'mic', text: 'text', static: 'layers' };

export const iconOf = (msg) => ICON_OF[kindOf(msg)] || 'chat';

export function titleOf(msg) {
  const secs = msg.duration ? ` · ${Math.round(msg.duration)}s` : '';
  if (isStatic(msg)) return 'Tin nhắn tĩnh';
  switch (msg.type) {
    case 'video': return `Video${secs}`;
    case 'voice': return `Lời nhắn thoại${secs}`;
    case 'image': return 'Ảnh';
    case 'gif': return 'Ảnh động';
    case 'text': return 'Dòng chữ';
    default: return 'Tin nhắn';
  }
}
