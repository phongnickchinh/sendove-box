// ==================================================
// The box's standby-screen theme — node: boxes/{box_id}/config/theme
// ==================================================
//
// The shape follows what the firmware's LayoutEngine reads
// (sendlove_firmware/lib/LayoutEngine/LayoutEngine.cpp): a "widgets" array of
// {type, x, y, w, h, color, align, font}.

/** Types the firmware can draw. "image" is excluded: the firmware has `case WIDGET_IMAGE: break;`. */
export type ThemeWidgetType = 'clock_time' | 'clock_date' | 'battery_icon' | 'wifi_icon' | 'chip_temp';

export type ThemeAlign = 'left' | 'center' | 'right';

export interface ThemeWidget {
  type: ThemeWidgetType;
  x: number;
  y: number;
  w: number;
  h: number;
  /** #RRGGBB — hexToColor() requires exactly 7 characters */
  color?: string;
  align?: ThemeAlign;
  /**
   * 'f_time' / 'f_date' = VLW fonts subset by the web, shipped in the theme's assets.
   * 'Font7' (7-segment time) / 'Font2' (small ASCII) = fonts built into the firmware.
   */
  font?: string;
  /** Web font family subset into the VLW file + size (px). Read only by the web, to reopen the editor. */
  family?: string;
  px?: number;
  /** clock_date: 'WD, DD.MM' | 'WD DD.MM' | 'DD/MM/YYYY' (LayoutEngine::formatDate) */
  format?: string;
  /** clock_date: weekday names in Vietnamese ('vi') or English ('en') */
  locale?: 'vi' | 'en';
}

/** One file of a theme package. size + crc32 are measured by the backend; the box checks them before accepting. */
export interface ThemeAsset {
  path: string;
  size: number;
  crc32: number;
}

export interface BoxTheme {
  /**
   * Package identity + revision. The box compares (theme_id, rev) with the copy
   * in its `theme` flash partition to decide whether to download again;
   * theme_flag alone isn't enough (the flag can be cleared before the box
   * finishes installing). Themes saved before the SD-card design lack both fields.
   */
  theme_id?: string;
  rev?: number;
  theme_name: string;
  widgets: ThemeWidget[];
  /**
   * Storage path of the background: raw 240×240 little-endian RGB565 = 115,200
   * bytes, under media/{boxId}/theme/ so the box can read it under the existing
   * storage.rules. null = black background (the firmware has no built-in one).
   */
  background: string | null;
  /** bg / f_time / f_date — what the box downloads to the card, then copies to flash. */
  assets?: Partial<Record<'bg' | 'f_time' | 'f_date', ThemeAsset>>;
  updated_at: number;
  updated_by: string;
}
