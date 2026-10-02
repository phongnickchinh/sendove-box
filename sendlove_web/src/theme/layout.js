/**
 * Theme contract for the box's standby screen — must match the backend
 * ThemeService (theme.service.ts) and the firmware's LayoutEngine::loadTheme().
 * The firmware compiles in NO background, fonts or layout (firmware MEMORY.md
 * §28): everything ships in the theme package.
 *
 * Fonts: 'f_time' / 'f_date' = VLW files subset by the web (utils/vlw.js) for
 * time / date widgets; 'Font7' / 'Font2' = firmware built-ins.
 */

/** config.h SCREEN_WIDTH/HEIGHT. */
export const SCREEN = 240;

/** The background is an RGB565 array: 240 × 240 × 2 bytes — the backend requires this exact size. */
export const BG_BYTES = SCREEN * SCREEN * 2; // 115,200

/** Default background (exported from the firmware's old StandbyBackground[]), served statically with the web. */
export const DEFAULT_BG_URL = `${import.meta.env.BASE_URL}theme/default-bg.bin`;

/** Google Fonts families with full Vietnamese coverage; the web subsets them to VLW at the chosen size. */
export const FONT_FAMILIES = [
  { family: 'Chakra Petch', weight: 600, label: 'Chakra Petch' },
  { family: 'Be Vietnam Pro', weight: 600, label: 'Be Vietnam Pro' },
  { family: 'Oswald', weight: 500, label: 'Oswald' },
];

/** Fonts built into the firmware (no font file to download). */
export const BUILTIN_FONTS = {
  clock_time: { value: 'Font7', label: 'Số 7 đoạn (có sẵn)' },
  clock_date: { value: 'Font2', label: 'Chữ nhỏ có sẵn, không dấu' },
};

/** Font file name inside the package, by widget type. */
export const VLW_KEY = { clock_time: 'f_time', clock_date: 'f_date' };

/** Allowed font sizes. The ceiling is real: the box allocates each glyph on an 8KB stack (~2-3KB at 56px). */
export const PX_RANGE = { clock_time: [24, 56], clock_date: [12, 28] };

/** LayoutEngine::formatDate — MUST match character for character (the web subsets the font from these strings). */
export const DATE_FORMATS = [
  { value: 'WD, DD.MM', label: 'Thứ, ngày.tháng' },
  { value: 'WD DD.MM', label: 'Thứ ngày.tháng' },
  { value: 'DD/MM/YYYY', label: 'ngày/tháng/năm' },
];
export const WEEKDAYS = {
  vi: ['Chủ nhật', 'Thứ hai', 'Thứ ba', 'Thứ tư', 'Thứ năm', 'Thứ sáu', 'Thứ bảy'],
  viAscii: ['CN', 'T2', 'T3', 'T4', 'T5', 'T6', 'T7'],
  en: ['Sun', 'Mon', 'Tue', 'Wed', 'Thu', 'Fri', 'Sat'],
};

/** drawTextWidget: 'center' / 'right' / anything else is left. */
export const ALIGNS = [
  { value: 'left', icon: 'alignL', label: 'Căn trái' },
  { value: 'center', icon: 'alignC', label: 'Căn giữa' },
  { value: 'right', icon: 'alignR', label: 'Căn phải' },
];

/**
 * Every widget type the firmware can draw. `color`/`align` are set only where
 * the firmware reads them (battery_icon ignores color; wifi_icon has no align).
 * min = floor for w×h: the firmware clears exactly that box before redrawing, so
 * a box narrower than the text leaves stale pixels.
 */
export const WIDGET_TYPES = {
  clock_time: { label: 'Giờ', icon: 'clock', sample: '21:47', color: true, align: true, min: { w: 100, h: 30 }, size: { w: 160, h: 50 } },
  clock_date: { label: 'Ngày', icon: 'info', color: true, align: true, min: { w: 80, h: 14 }, size: { w: 160, h: 24 } },
  battery_icon: { label: 'Pin', icon: 'battery', color: false, align: false, min: { w: 75, h: 16 }, size: { w: 75, h: 16 }, fixedSize: true },
  wifi_icon: { label: 'Sóng Wi-Fi', icon: 'wifi', color: true, align: false, min: { w: 24, h: 18 }, size: { w: 24, h: 18 } },
  chip_temp: { label: 'Nhiệt độ chip', icon: 'gear', sample: "41'C", color: true, align: true, min: { w: 100, h: 20 }, size: { w: 100, h: 20 } },
};

export const MAX_WIDGETS = 8;

// Guard font-less types (battery, Wi-Fi): undefined === undefined would treat them as font widgets.
export const isVlw = (w) => !!VLW_KEY[w.type] && w.font === VLW_KEY[w.type];

/** The date as the box will draw it at `now`. */
export function formatDate(w, now = new Date()) {
  const vlw = isVlw(w);
  const names = w.locale === 'en' ? WEEKDAYS.en : vlw ? WEEKDAYS.vi : WEEKDAYS.viAscii;
  const wd = names[now.getDay()];
  const dd = String(now.getDate()).padStart(2, '0');
  const mm = String(now.getMonth() + 1).padStart(2, '0');
  if (w.format === 'DD/MM/YYYY') return `${dd}/${mm}/${now.getFullYear()}`;
  if (w.format === 'WD DD.MM') return `${wd} ${dd}.${mm}`;
  return `${wd}, ${dd}.${mm}`;
}

/** Every character the widgets may need (all 7 weekdays, all digits) — the charset for VLW subsetting. */
export function charsetFor(type, widgets) {
  if (type === 'clock_time') return '0123456789:-';
  const set = new Set('0123456789.,/- ');
  for (const w of widgets.filter((x) => x.type === 'clock_date' && isVlw(x))) {
    const names = w.locale === 'en' ? WEEKDAYS.en : WEEKDAYS.vi;
    names.join('').normalize('NFC').split('').forEach((c) => set.add(c));
  }
  set.delete(' '); // LovyanGFX draws spaces from spaceWidth, not from a glyph
  return [...set].join('');
}

/** API widgets → editor widgets (adds id/label/icon). Legacy font names map to the built-in font. */
export function toEditorWidgets(apiWidgets) {
  return apiWidgets.map((w, i) => {
    const meta = WIDGET_TYPES[w.type] || {};
    const out = { ...w, id: `${w.type}_${i}`, label: meta.label || w.type, icon: meta.icon || 'layers', sample: meta.sample };
    if (VLW_KEY[w.type]) {
      // Themes saved before the SD-card design name compiled-in fonts that no longer exist → fall back to the built-in.
      if (w.font !== VLW_KEY[w.type] && w.font !== BUILTIN_FONTS[w.type].value) out.font = BUILTIN_FONTS[w.type].value;
      out.family = w.family || FONT_FAMILIES[0].family;
      out.px = w.px || (w.type === 'clock_time' ? 44 : 18);
    }
    if (w.type === 'clock_date') {
      out.format = w.format || DATE_FORMATS[0].value;
      out.locale = w.locale || 'vi';
    }
    return out;
  });
}

/** Editor widgets → exactly the keys the backend accepts (the backend filters again). */
export function toApiWidgets(widgets) {
  return widgets.map((w) => {
    const meta = WIDGET_TYPES[w.type];
    const out = { type: w.type, x: w.x, y: w.y, w: w.w, h: w.h };
    if (meta?.color) out.color = w.color || '#000000';
    if (meta?.align) out.align = w.align || 'left';
    if (VLW_KEY[w.type]) {
      out.font = w.font;
      if (isVlw(w)) {
        out.family = w.family;
        out.px = w.px;
      }
    }
    if (w.type === 'clock_date') {
      out.format = w.format;
      out.locale = w.locale;
    }
    return out;
  });
}

/** A new widget, centered, at its type's default size. */
export function newWidget(type, existing) {
  const meta = WIDGET_TYPES[type];
  const { w, h } = meta.size;
  const n = existing.filter((x) => x.type === type).length;
  return {
    id: `${type}_${Date.now()}`, type, label: meta.label, icon: meta.icon, sample: meta.sample,
    x: Math.round((SCREEN - w) / 2), y: Math.min(SCREEN - h, 100 + n * 10), w, h,
    ...(meta.color && { color: '#000000' }),
    ...(meta.align && { align: 'center' }),
    ...(VLW_KEY[type] && {
      font: VLW_KEY[type], family: FONT_FAMILIES[0].family, px: type === 'clock_time' ? 44 : 18,
    }),
    ...(type === 'clock_date' && { format: DATE_FORMATS[0].value, locale: 'vi' }),
  };
}

/** A widget's validation error, by the backend's rules — null if valid. */
export function widgetProblem(w) {
  const meta = WIDGET_TYPES[w.type];
  if (!meta) return 'Loại widget này firmware không vẽ được.';
  for (const k of ['x', 'y', 'w', 'h']) {
    if (!Number.isInteger(w[k]) || w[k] < 0 || w[k] > SCREEN) return `${k.toUpperCase()} phải là số nguyên 0-${SCREEN}.`;
  }
  if (w.x + w.w > SCREEN || w.y + w.h > SCREEN) return `Ô tràn ra ngoài màn ${SCREEN} × ${SCREEN}.`;
  if (meta.color && !/^#[0-9A-Fa-f]{6}$/.test(w.color || '')) return 'Màu phải viết đủ dạng #RRGGBB.';
  if (isVlw(w)) {
    const [lo, hi] = PX_RANGE[w.type];
    if (!(w.px >= lo && w.px <= hi)) return `Cỡ chữ phải từ ${lo} đến ${hi}.`;
  }
  return null;
}

/**
 * Widgets of one type SHARE one font file, taken from the first widget's family
 * + size. Returns a message when another widget differs.
 */
export function sharedFontIssue(widgets) {
  for (const type of Object.keys(VLW_KEY)) {
    const list = widgets.filter((w) => w.type === type && isVlw(w));
    if (list.length > 1 && list.some((w) => w.family !== list[0].family || w.px !== list[0].px)) {
      return `Các widget ${WIDGET_TYPES[type].label.toLowerCase()} dùng chung một phông: hộp vẽ tất cả theo ${list[0].family} ${list[0].px}px.`;
    }
  }
  return null;
}

/** Default layout — the look that used to be compiled into the firmware, now shipped as a theme package. */
const DEFAULT_API = [
  { type: 'clock_date', x: 9, y: 9, w: 150, h: 22, color: '#B83D3D', align: 'left', font: 'f_date', family: 'Chakra Petch', px: 18, format: 'WD, DD.MM', locale: 'vi' },
  { type: 'clock_time', x: 40, y: 30, w: 180, h: 50, color: '#000000', align: 'center', font: 'f_time', family: 'Chakra Petch', px: 44 },
  { type: 'battery_icon', x: 154, y: 10, w: 75, h: 16 },
];

export const DEFAULT_WIDGETS = toEditorWidgets(DEFAULT_API);

/** Presets — all real layouts that can be previewed and sent. `defaultBg` = use the default background. */
export const PRESETS = [
  { id: 'default', name: 'Mặc định', what: 'Ngày, giờ lớn, biểu tượng pin — giao diện gốc của hộp', widgets: DEFAULT_API, defaultBg: true },
  {
    id: 'big_clock', name: 'Đồng hồ lớn', what: 'Chỉ có giờ, phông Oswald, căn giữa màn',
    widgets: [
      { type: 'clock_time', x: 10, y: 90, w: 220, h: 60, color: '#3D2A20', align: 'center', font: 'f_time', family: 'Oswald', px: 56 },
    ],
    defaultBg: true,
  },
  {
    id: 'quiet', name: 'Tối giản', what: 'Giờ và ngày xếp dưới, góc trái',
    widgets: [
      { type: 'clock_time', x: 16, y: 150, w: 160, h: 50, color: '#3D2A20', align: 'left', font: 'f_time', family: 'Be Vietnam Pro', px: 44 },
      { type: 'clock_date', x: 16, y: 204, w: 180, h: 22, color: '#83513E', align: 'left', font: 'f_date', family: 'Be Vietnam Pro', px: 16, format: 'WD, DD.MM', locale: 'vi' },
    ],
    defaultBg: true,
  },
];
