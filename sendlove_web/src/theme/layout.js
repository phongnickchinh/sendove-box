/**
 * Hợp đồng theme cho màn chờ của hộp — khớp backend ThemeService
 * (sendlove_backend/src/services/theme.service.ts, route /boxes/:boxId/theme)
 * và thứ LayoutEngine::loadConfig() của firmware đọc
 * (sendlove_firmware/lib/LayoutEngine/LayoutEngine.cpp).
 *
 * Firmware HIỆN TẠI vẫn vẽ bố cục biên dịch cứng (src/main.cpp:106-114) và
 * CHƯA tải theme từ tài khoản — giao diện lưu từ web nằm chờ trên DB cho tới
 * khi firmware đọc cờ theme_flag (xem memory sendlove-fw-todo-tu-fe mục 3).
 */

/** config.h SCREEN_WIDTH/HEIGHT, StandbyBackground.h BG_WIDTH/HEIGHT. */
export const SCREEN = 240;

/** Nền là mảng RGB565: 240 × 240 × 2 byte — backend đòi đúng số byte này. */
export const BG_BYTES = SCREEN * SCREEN * 2; // 115.200

/**
 * drawClockTime / drawClockDate là hai if-chain KHÁC NHAU. Tên phông của
 * widget này đưa sang widget kia sẽ âm thầm rơi về mặc định, nên danh sách
 * phông đi theo từng type (backend kiểm y hệt).
 */
export const FONTS_BY_TYPE = {
  clock_time: [
    { value: 'ChakraPetch_SemiBold_48', label: 'Chakra Petch 48' },
    { value: 'Orbitron_32', label: 'Orbitron 32' },
    { value: 'Font7', label: 'Số 7 đoạn' },
  ],
  clock_date: [
    { value: 'ChakraPetch_SemiBold_16', label: 'Chakra Petch 16' },
    // Tên trong firmware là "Roboto_14" nhưng thật ra nạp Roboto_Thin_24.
    { value: 'Roboto_14', label: 'Roboto mảnh 24' },
    { value: 'FreeSans_12', label: 'FreeSans đậm 12' },
  ],
};

/** drawTextWidget: 'center' / 'right' / còn lại đều là trái. */
export const ALIGNS = [
  { value: 'left', icon: 'alignL', label: 'Căn trái' },
  { value: 'center', icon: 'alignC', label: 'Căn giữa' },
  { value: 'right', icon: 'alignR', label: 'Căn phải' },
];

/**
 * Mô tả từng loại widget firmware vẽ được. `color`/`align` chỉ có khi firmware
 * thật sự đọc trường đó cho loại này:
 *   battery_icon — pushImage một ảnh nhiều màu, BỎ QUA cfg.color; mức pin
 *                  viết cứng `int state = 3;`.
 *   wifi_icon    — drawBitmap 24px, CÓ dùng color, không căn lề.
 *   chip_temp    — chữ ChakraPetch 16 cố định, dùng color + align.
 * min = sàn w×h: drawTextWidget xoá đúng ô w×h trước khi vẽ lại, ô hẹp hơn chữ
 * thì phần thừa của lần vẽ trước dính lại trên màn.
 */
export const WIDGET_TYPES = {
  clock_time: { label: 'Giờ', icon: 'clock', sample: '21:47', color: true, align: true, min: { w: 132, h: 35 }, size: { w: 160, h: 45 } },
  clock_date: { label: 'Ngày', icon: 'info', sample: 'Sat, 29.08', color: true, align: true, min: { w: 140, h: 16 }, size: { w: 140, h: 16 } },
  battery_icon: { label: 'Pin', icon: 'battery', color: false, align: false, min: { w: 75, h: 16 }, size: { w: 75, h: 16 }, fixedSize: true },
  wifi_icon: { label: 'Sóng Wi-Fi', icon: 'wifi', color: true, align: false, min: { w: 24, h: 18 }, size: { w: 24, h: 18 } },
  chip_temp: { label: 'Nhiệt độ chip', icon: 'gear', sample: "41'C", color: true, align: true, min: { w: 100, h: 20 }, size: { w: 100, h: 20 } },
};

export const MAX_WIDGETS = 8;

/** Widget thô (dạng API) → widget cho editor (thêm id/label/icon/sample). */
export function toEditorWidgets(apiWidgets) {
  return apiWidgets.map((w, i) => {
    const meta = WIDGET_TYPES[w.type] || {};
    return { ...w, id: `${w.type}_${i}`, label: meta.label || w.type, icon: meta.icon || 'layers', sample: meta.sample };
  });
}

/** Widget của editor → đúng các khoá firmware đọc (backend cũng lọc lại). */
export function toApiWidgets(widgets) {
  return widgets.map((w) => {
    const meta = WIDGET_TYPES[w.type];
    const out = { type: w.type, x: w.x, y: w.y, w: w.w, h: w.h };
    if (meta?.color) out.color = w.color || '#000000';
    if (meta?.align) out.align = w.align || 'left';
    if (FONTS_BY_TYPE[w.type]) out.font = w.font || FONTS_BY_TYPE[w.type][0].value;
    return out;
  });
}

/** Widget mới đặt giữa màn, cỡ mặc định của loại đó. */
export function newWidget(type, existing) {
  const meta = WIDGET_TYPES[type];
  const { w, h } = meta.size;
  const n = existing.filter((x) => x.type === type).length;
  return {
    id: `${type}_${Date.now()}`, type, label: meta.label, icon: meta.icon, sample: meta.sample,
    x: Math.round((SCREEN - w) / 2), y: Math.min(SCREEN - h, 100 + n * 10), w, h,
    ...(meta.color && { color: '#000000' }),
    ...(meta.align && { align: 'center' }),
    ...(FONTS_BY_TYPE[type] && { font: FONTS_BY_TYPE[type][0].value }),
  };
}

/** Lỗi của một widget theo đúng luật backend — null nếu hợp lệ. */
export function widgetProblem(w) {
  const meta = WIDGET_TYPES[w.type];
  if (!meta) return 'Loại widget này firmware không vẽ được.';
  for (const k of ['x', 'y', 'w', 'h']) {
    if (!Number.isInteger(w[k]) || w[k] < 0 || w[k] > SCREEN) return `${k.toUpperCase()} phải là số nguyên 0-${SCREEN}.`;
  }
  if (w.x + w.w > SCREEN || w.y + w.h > SCREEN) return `Ô tràn ra ngoài màn ${SCREEN} × ${SCREEN}.`;
  if (meta.color && !/^#[0-9A-Fa-f]{6}$/.test(w.color || '')) return 'Màu phải viết đủ dạng #RRGGBB.';
  return null;
}

/** Đúng defaultLayoutJson biên dịch cứng trong firmware (src/main.cpp:106-114). */
const DEFAULT_API = [
  { type: 'clock_date', x: 9, y: 9, w: 140, h: 16, color: '#B83D3D', align: 'left', font: 'ChakraPetch_SemiBold_16' },
  { type: 'clock_time', x: 50, y: 30, w: 160, h: 45, color: '#000000', align: 'center', font: 'ChakraPetch_SemiBold_48' },
  { type: 'battery_icon', x: 154, y: 10, w: 75, h: 16 },
];

export const DEFAULT_WIDGETS = toEditorWidgets(DEFAULT_API);

/** Mẫu có sẵn — đều là bố cục thật, xem trước được và gửi được. */
export const PRESETS = [
  { id: 'default', name: 'Mặc định', what: 'Ngày, giờ lớn, biểu tượng pin — giống bản trong firmware', widgets: DEFAULT_API },
  {
    id: 'big_clock', name: 'Đồng hồ lớn', what: 'Chỉ có giờ, phông Orbitron, căn giữa màn',
    widgets: [
      { type: 'clock_time', x: 20, y: 95, w: 200, h: 50, color: '#3D2A20', align: 'center', font: 'Orbitron_32' },
    ],
  },
  {
    id: 'quiet', name: 'Tối giản', what: 'Giờ và ngày xếp dưới, góc trái',
    widgets: [
      { type: 'clock_time', x: 16, y: 150, w: 160, h: 45, color: '#3D2A20', align: 'left', font: 'ChakraPetch_SemiBold_48' },
      { type: 'clock_date', x: 16, y: 200, w: 150, h: 16, color: '#83513E', align: 'left', font: 'ChakraPetch_SemiBold_16' },
    ],
  },
];
