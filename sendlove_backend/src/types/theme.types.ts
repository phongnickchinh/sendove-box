// ==================================================
// Theme màn hình chờ của hộp — Node: boxes/{box_id}/config/theme
// ==================================================
//
// Hình dạng bám đúng thứ LayoutEngine::loadConfig() của firmware đọc
// (sendlove_firmware/lib/LayoutEngine/LayoutEngine.cpp): mảng "widgets", mỗi
// phần tử {type, x, y, w, h, color, align, font}. Firmware hiện tại CHƯA tải
// theme từ cloud — xem memory sendlove-fw-todo-tu-fe mục 3.

/** Các type firmware vẽ được. "image" bị bỏ vì firmware có `case WIDGET_IMAGE: break;`. */
export type ThemeWidgetType = 'clock_time' | 'clock_date' | 'battery_icon' | 'wifi_icon' | 'chip_temp';

export type ThemeAlign = 'left' | 'center' | 'right';

export interface ThemeWidget {
  type: ThemeWidgetType;
  x: number;
  y: number;
  w: number;
  h: number;
  /** #RRGGBB — hexToColor() đòi đúng 7 ký tự */
  color?: string;
  align?: ThemeAlign;
  /**
   * 'f_time' / 'f_date' = phông VLW web cắt sẵn, nằm trong assets của theme.
   * 'Font7' (giờ 7 đoạn) / 'Font2' (chữ nhỏ ASCII) = phông có sẵn trong firmware.
   */
  font?: string;
  /** Họ phông web đã cắt ra file VLW + cỡ chữ (px). Chỉ web đọc, để mở lại trình sửa. */
  family?: string;
  px?: number;
  /** clock_date: 'WD, DD.MM' | 'WD DD.MM' | 'DD/MM/YYYY' (LayoutEngine::formatDate) */
  format?: string;
  /** clock_date: tên thứ tiếng Việt ('vi') hay tiếng Anh ('en') */
  locale?: 'vi' | 'en';
}

/** Một file của gói theme. size + crc32 backend tự đo; hộp kiểm trước khi nhận. */
export interface ThemeAsset {
  path: string;
  size: number;
  crc32: number;
}

export interface BoxTheme {
  /**
   * Định danh + số bản của gói. Hộp so (theme_id, rev) với bản trong phân vùng flash
   * `theme` để biết có phải tải lại không; theme_flag một mình không đủ (cờ có thể bị hạ
   * trước khi hộp cài xong). Theme lưu trước 2026-09-24 không có hai trường này.
   */
  theme_id?: string;
  rev?: number;
  theme_name: string;
  widgets: ThemeWidget[];
  /**
   * Storage path của ảnh nền: 240×240 RGB565 little-endian thô = 115.200 byte,
   * nằm dưới media/{boxId}/theme/ để box đọc được theo storage.rules hiện có.
   * null = nền đen (firmware không còn nền biên dịch sẵn từ 2026-09-24).
   */
  background: string | null;
  /** bg / f_time / f_date — thứ hộp tải về thẻ rồi chép sang flash. */
  assets?: Partial<Record<'bg' | 'f_time' | 'f_date', ThemeAsset>>;
  updated_at: number;
  updated_by: string;
}
