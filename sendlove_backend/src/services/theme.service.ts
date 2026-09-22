import { IBoxRepository } from '../repositories/interfaces/box.repository.interface';
import { IStorageRepository } from '../repositories/interfaces/storage.repository.interface';
import { FirebaseBoxRepository } from '../repositories/firebase/firebase-box.repository';
import { FirebaseStorageRepository } from '../repositories/firebase/firebase-storage.repository';
import { AppError } from '../middleware/error-handler.middleware';
import { BoxTheme, ThemeWidget, ThemeWidgetType } from '../types/theme.types';

export const SCREEN = 240;
/** 240 × 240 × 2 byte RGB565 — đúng kích thước mảng StandbyBackground của firmware. */
export const BG_BYTES = SCREEN * SCREEN * 2;
const MAX_WIDGETS = 8;

/**
 * Luật theo từng type — chỉ nhận đúng khoá firmware đọc cho type đó, và phông
 * nằm trong if-chain thật của drawClockTime/drawClockDate. Tên phông khác thì
 * firmware âm thầm rơi về mặc định, nên chặn ở đây thay vì để người dùng tưởng
 * đã đổi được.
 */
const RULES: Record<ThemeWidgetType, { color: boolean; align: boolean; fonts?: string[] }> = {
  clock_time: { color: true, align: true, fonts: ['ChakraPetch_SemiBold_48', 'Orbitron_32', 'Font7'] },
  clock_date: { color: true, align: true, fonts: ['ChakraPetch_SemiBold_16', 'Roboto_14', 'FreeSans_12'] },
  chip_temp: { color: true, align: true },
  wifi_icon: { color: true, align: false },
  battery_icon: { color: false, align: false }, // pushImage nhiều màu, bỏ qua cfg.color
};

const HEX = /^#[0-9A-Fa-f]{6}$/;
const isInt = (v: unknown): v is number => typeof v === 'number' && Number.isInteger(v);

/** Kiểm tra + chuẩn hoá widgets; ném 400 với lý do đọc được. */
export function sanitizeWidgets(input: unknown): ThemeWidget[] {
  if (!Array.isArray(input) || input.length === 0) {
    throw new AppError(400, 'invalid_theme', "'widgets' must be a non-empty array");
  }
  if (input.length > MAX_WIDGETS) {
    throw new AppError(400, 'invalid_theme', `At most ${MAX_WIDGETS} widgets`);
  }

  return input.map((raw: any, i: number) => {
    const where = `widgets[${i}]`;
    if (!raw || typeof raw !== 'object') throw new AppError(400, 'invalid_theme', `${where} must be an object`);

    const type = raw.type as ThemeWidgetType;
    const rule = RULES[type];
    // LayoutEngine.cpp strcmp(type, ...) không kiểm tra null — widget thiếu type làm sập hộp.
    if (!rule) throw new AppError(400, 'invalid_theme', `${where}.type must be one of: ${Object.keys(RULES).join(', ')}`);

    for (const k of ['x', 'y', 'w', 'h'] as const) {
      if (!isInt(raw[k]) || raw[k] < 0 || raw[k] > SCREEN) {
        throw new AppError(400, 'invalid_theme', `${where}.${k} must be an integer 0-${SCREEN}`);
      }
    }
    if (raw.w < 1 || raw.h < 1) throw new AppError(400, 'invalid_theme', `${where} must have w, h >= 1`);
    if (raw.x + raw.w > SCREEN || raw.y + raw.h > SCREEN) {
      throw new AppError(400, 'invalid_theme', `${where} goes outside the ${SCREEN}x${SCREEN} screen`);
    }

    const out: ThemeWidget = { type, x: raw.x, y: raw.y, w: raw.w, h: raw.h };

    if (rule.color) {
      // Thiếu màu thì firmware tô ĐEN (mặc định "#000000"), không phải trắng — ghi rõ ra.
      const color = raw.color ?? '#000000';
      if (typeof color !== 'string' || !HEX.test(color)) {
        throw new AppError(400, 'invalid_theme', `${where}.color must be #RRGGBB`);
      }
      out.color = color.toUpperCase();
    }
    if (rule.align) {
      const align = raw.align ?? 'left';
      if (!['left', 'center', 'right'].includes(align)) {
        throw new AppError(400, 'invalid_theme', `${where}.align must be left, center or right`);
      }
      out.align = align;
    }
    if (rule.fonts) {
      const font = raw.font ?? rule.fonts[0];
      if (!rule.fonts.includes(font)) {
        throw new AppError(400, 'invalid_theme', `${where}.font must be one of: ${rule.fonts.join(', ')}`);
      }
      out.font = font;
    }
    return out;
  });
}

export class ThemeService {
  constructor(
    private boxRepo: IBoxRepository = new FirebaseBoxRepository(),
    private storageRepo: IStorageRepository = new FirebaseStorageRepository()
  ) {}

  private bgPrefix = (boxId: string) => `media/${boxId}/theme/`;

  async getTheme(boxId: string): Promise<BoxTheme | null> {
    const box = await this.boxRepo.getById(boxId);
    if (!box) throw new AppError(404, 'box_not_found', 'Box not found');
    return ((box.config as any)?.theme as BoxTheme) || null;
  }

  /** Cấp signed POST policy để web tải ảnh nền lên thẳng Storage. */
  async initiateBackgroundUpload(boxId: string) {
    const path = `${this.bgPrefix(boxId)}bg_${Date.now()}.bin`;
    const upload = await this.storageRepo.generateUploadPolicy(path, 'application/octet-stream', BG_BYTES, 15);
    return { path, upload };
  }

  async saveTheme(boxId: string, uid: string, body: any): Promise<BoxTheme> {
    const name = typeof body?.theme_name === 'string' ? body.theme_name.trim() : '';
    if (name.length < 1 || name.length > 40) {
      throw new AppError(400, 'invalid_theme', "'theme_name' must be 1-40 characters");
    }
    const widgets = sanitizeWidgets(body?.widgets);

    let background: string | null = null;
    if (body?.background != null) {
      const path = body.background;
      // Chỉ nhận file nằm đúng thư mục theme của CHÍNH hộp này — không cho trỏ
      // sang media của hộp khác hay tin nhắn.
      if (typeof path !== 'string' || !path.startsWith(this.bgPrefix(boxId)) || !/^[\w./-]+\.bin$/.test(path) || path.includes('..')) {
        throw new AppError(400, 'invalid_theme', "'background' must be a path returned by /theme/background");
      }
      const exists = await this.storageRepo.fileExists(path);
      const size = exists ? parseInt((await this.storageRepo.getFileMetadata(path))?.size || '0', 10) : 0;
      if (size !== BG_BYTES) {
        throw new AppError(400, 'invalid_theme', `Background must be exactly ${BG_BYTES} bytes (240x240 RGB565), got ${size}`);
      }
      background = path;
    }

    const theme: BoxTheme = { theme_name: name, widgets, background, updated_at: Date.now(), updated_by: uid };
    await this.boxRepo.update(boxId, { 'config/theme': theme, updated_at: theme.updated_at } as any);
    await this.boxRepo.updateFlags(boxId, { theme_flag: true });
    return theme;
  }
}
