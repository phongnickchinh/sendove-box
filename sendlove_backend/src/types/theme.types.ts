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
  font?: string;
}

export interface BoxTheme {
  theme_name: string;
  widgets: ThemeWidget[];
  /**
   * Storage path của ảnh nền: 240×240 RGB565 little-endian thô = 115.200 byte,
   * nằm dưới media/{boxId}/theme/ để box đọc được theo storage.rules hiện có.
   * null = dùng nền biên dịch sẵn trong firmware.
   */
  background: string | null;
  updated_at: number;
  updated_by: string;
}
