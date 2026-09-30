"use strict";
Object.defineProperty(exports, "__esModule", { value: true });
exports.ThemeService = exports.DATE_FORMATS = exports.FONT_MAX_BYTES = exports.BG_BYTES = exports.SCREEN = void 0;
exports.sanitizeWidgets = sanitizeWidgets;
const firebase_box_repository_1 = require("../repositories/firebase/firebase-box.repository");
const firebase_storage_repository_1 = require("../repositories/firebase/firebase-storage.repository");
const error_handler_middleware_1 = require("../middleware/error-handler.middleware");
const crc32_1 = require("../utils/crc32");
exports.SCREEN = 240;
/** 240 × 240 × 2 byte RGB565 — đúng kích thước nền LayoutEngine đọc từ phân vùng theme. */
exports.BG_BYTES = exports.SCREEN * exports.SCREEN * 2;
/** Phông VLW web cắt sẵn đúng tập ký tự cần dùng: vài chục glyph, vài chục KB là nhiều. */
exports.FONT_MAX_BYTES = 64 * 1024;
const MAX_WIDGETS = 8;
/** LayoutEngine::formatDate — web cắt phông theo đúng các chuỗi này. */
exports.DATE_FORMATS = ['WD, DD.MM', 'WD DD.MM', 'DD/MM/YYYY'];
const LOCALES = ['vi', 'en'];
/**
 * Luật theo từng type — chỉ nhận đúng khoá firmware đọc cho type đó.
 * Phông (2026-09-24): 'f_time'/'f_date' = VLW trong gói theme (phải kèm file trong `fonts`),
 * 'Font7'/'Font2' = phông có sẵn trong firmware. ChakraPetch/Orbitron/Roboto biên dịch
 * cứng đã GỠ khỏi firmware, tên cũ firmware sẽ âm thầm vẽ bằng phông có sẵn -> chặn ở đây.
 */
const RULES = {
    clock_time: { color: true, align: true, fonts: ['f_time', 'Font7'] },
    clock_date: { color: true, align: true, fonts: ['f_date', 'Font2'], date: true },
    chip_temp: { color: true, align: true },
    wifi_icon: { color: true, align: false },
    battery_icon: { color: false, align: false }, // pushImage nhiều màu, bỏ qua cfg.color
};
const HEX = /^#[0-9A-Fa-f]{6}$/;
const isInt = (v) => typeof v === 'number' && Number.isInteger(v);
/** Kiểm tra + chuẩn hoá widgets; ném 400 với lý do đọc được. */
function sanitizeWidgets(input) {
    if (!Array.isArray(input) || input.length === 0) {
        throw new error_handler_middleware_1.AppError(400, 'invalid_theme', "'widgets' must be a non-empty array");
    }
    if (input.length > MAX_WIDGETS) {
        throw new error_handler_middleware_1.AppError(400, 'invalid_theme', `At most ${MAX_WIDGETS} widgets`);
    }
    return input.map((raw, i) => {
        const where = `widgets[${i}]`;
        if (!raw || typeof raw !== 'object')
            throw new error_handler_middleware_1.AppError(400, 'invalid_theme', `${where} must be an object`);
        const type = raw.type;
        const rule = RULES[type];
        // LayoutEngine.cpp strcmp(type, ...) không kiểm tra null — widget thiếu type làm sập hộp.
        if (!rule)
            throw new error_handler_middleware_1.AppError(400, 'invalid_theme', `${where}.type must be one of: ${Object.keys(RULES).join(', ')}`);
        for (const k of ['x', 'y', 'w', 'h']) {
            if (!isInt(raw[k]) || raw[k] < 0 || raw[k] > exports.SCREEN) {
                throw new error_handler_middleware_1.AppError(400, 'invalid_theme', `${where}.${k} must be an integer 0-${exports.SCREEN}`);
            }
        }
        if (raw.w < 1 || raw.h < 1)
            throw new error_handler_middleware_1.AppError(400, 'invalid_theme', `${where} must have w, h >= 1`);
        if (raw.x + raw.w > exports.SCREEN || raw.y + raw.h > exports.SCREEN) {
            throw new error_handler_middleware_1.AppError(400, 'invalid_theme', `${where} goes outside the ${exports.SCREEN}x${exports.SCREEN} screen`);
        }
        const out = { type, x: raw.x, y: raw.y, w: raw.w, h: raw.h };
        if (rule.color) {
            // Thiếu màu thì firmware tô ĐEN (mặc định "#000000"), không phải trắng — ghi rõ ra.
            const color = raw.color ?? '#000000';
            if (typeof color !== 'string' || !HEX.test(color)) {
                throw new error_handler_middleware_1.AppError(400, 'invalid_theme', `${where}.color must be #RRGGBB`);
            }
            out.color = color.toUpperCase();
        }
        if (rule.align) {
            const align = raw.align ?? 'left';
            if (!['left', 'center', 'right'].includes(align)) {
                throw new error_handler_middleware_1.AppError(400, 'invalid_theme', `${where}.align must be left, center or right`);
            }
            out.align = align;
        }
        if (rule.fonts) {
            // Mặc định là phông có sẵn (phần tử cuối): không cần file phông nào.
            const font = raw.font ?? rule.fonts[rule.fonts.length - 1];
            if (!rule.fonts.includes(font)) {
                throw new error_handler_middleware_1.AppError(400, 'invalid_theme', `${where}.font must be one of: ${rule.fonts.join(', ')}`);
            }
            out.font = font;
            // Chỉ để web mở lại trình sửa đúng như lúc lưu (họ phông + cỡ đã cắt ra file VLW).
            // Firmware không đọc hai khoá này: hình chữ nằm sẵn trong file phông.
            if (typeof raw.family === 'string' && raw.family.length >= 1 && raw.family.length <= 40)
                out.family = raw.family;
            if (isInt(raw.px) && raw.px >= 8 && raw.px <= 72)
                out.px = raw.px;
        }
        if (rule.date) {
            const format = raw.format ?? exports.DATE_FORMATS[0];
            if (!exports.DATE_FORMATS.includes(format)) {
                throw new error_handler_middleware_1.AppError(400, 'invalid_theme', `${where}.format must be one of: ${exports.DATE_FORMATS.join(', ')}`);
            }
            const locale = raw.locale ?? 'vi';
            if (!LOCALES.includes(locale))
                throw new error_handler_middleware_1.AppError(400, 'invalid_theme', `${where}.locale must be vi or en`);
            out.format = format;
            out.locale = locale;
        }
        return out;
    });
}
class ThemeService {
    constructor(boxRepo = new firebase_box_repository_1.FirebaseBoxRepository(), storageRepo = new firebase_storage_repository_1.FirebaseStorageRepository()) {
        this.boxRepo = boxRepo;
        this.storageRepo = storageRepo;
        this.bgPrefix = (boxId) => `media/${boxId}/theme/`;
    }
    /** Theme đã lưu (null = chưa lưu lần nào), kèm URL ký 15 phút để web xem trước ảnh nền. */
    async getTheme(boxId) {
        const box = await this.boxRepo.getById(boxId);
        if (!box)
            throw new error_handler_middleware_1.AppError(404, 'box_not_found', 'Box not found');
        const theme = box.config?.theme;
        if (!theme)
            return null;
        let background_url = null;
        if (theme.background) {
            try {
                background_url = await this.storageRepo.generateDownloadUrl(theme.background, 15);
            }
            catch (error) {
                console.error(`[ThemeService] Failed to sign ${theme.background}`, error);
            }
        }
        return { ...theme, background_url };
    }
    /** Cấp signed POST policy để web tải ảnh nền lên thẳng Storage. */
    async initiateBackgroundUpload(boxId) {
        const path = `${this.bgPrefix(boxId)}bg_${Date.now()}.bin`;
        const upload = await this.storageRepo.generateUploadPolicy(path, 'application/octet-stream', exports.BG_BYTES, 15);
        return { path, upload };
    }
    /** Như ảnh nền, cho một file phông VLW (web cắt sẵn tập ký tự). */
    async initiateFontUpload(boxId) {
        const path = `${this.bgPrefix(boxId)}f_${Date.now()}_${Math.floor(Math.random() * 1000)}.vlw`;
        const upload = await this.storageRepo.generateUploadPolicy(path, 'application/octet-stream', exports.FONT_MAX_BYTES, 15);
        return { path, upload };
    }
    /**
     * Kiểm một file của gói (đúng thư mục theme của CHÍNH hộp này, đúng đuôi) rồi tự đo size
     * + crc32 từ file thật. Hộp dựa vào đúng hai số này để nhận file.
     */
    async measureAsset(boxId, path, ext, field) {
        const re = ext === 'bin' ? /^[\w./-]+\.bin$/ : /^[\w./-]+\.vlw$/;
        if (typeof path !== 'string' || !path.startsWith(this.bgPrefix(boxId)) || !re.test(path) || path.includes('..')) {
            throw new error_handler_middleware_1.AppError(400, 'invalid_theme', `'${field}' must be a path returned by /theme/${ext === 'bin' ? 'background' : 'font'}`);
        }
        if (!(await this.storageRepo.fileExists(path))) {
            throw new error_handler_middleware_1.AppError(400, 'invalid_theme', `'${field}' file not found`);
        }
        const buf = await this.storageRepo.downloadToBuffer(path);
        return { path, size: buf.length, crc32: (0, crc32_1.crc32)(buf) };
    }
    async saveTheme(boxId, uid, body) {
        const name = typeof body?.theme_name === 'string' ? body.theme_name.trim() : '';
        if (name.length < 1 || name.length > 40) {
            throw new error_handler_middleware_1.AppError(400, 'invalid_theme', "'theme_name' must be 1-40 characters");
        }
        const widgets = sanitizeWidgets(body?.widgets);
        const assets = {};
        let background = null;
        if (body?.background != null) {
            const bg = await this.measureAsset(boxId, body.background, 'bin', 'background');
            if (bg.size !== exports.BG_BYTES) {
                throw new error_handler_middleware_1.AppError(400, 'invalid_theme', `Background must be exactly ${exports.BG_BYTES} bytes (240x240 RGB565), got ${bg.size}`);
            }
            assets.bg = bg;
            background = bg.path;
        }
        // Phông VLW: widget dùng 'f_time'/'f_date' thì PHẢI kèm file tương ứng, không thì hộp
        // vẽ bằng phông có sẵn trong khi web xem trước bằng phông khác -> người dùng tưởng lỗi.
        for (const key of ['f_time', 'f_date']) {
            const used = widgets.some((w) => w.font === key);
            const path = body?.fonts?.[key];
            if (!used)
                continue;
            if (path == null)
                throw new error_handler_middleware_1.AppError(400, 'invalid_theme', `A widget uses '${key}' but fonts.${key} is missing`);
            const f = await this.measureAsset(boxId, path, 'vlw', `fonts.${key}`);
            if (f.size < 24 || f.size > exports.FONT_MAX_BYTES) {
                throw new error_handler_middleware_1.AppError(400, 'invalid_theme', `fonts.${key} must be 24-${exports.FONT_MAX_BYTES} bytes`);
            }
            assets[key] = f;
        }
        const box = await this.boxRepo.getById(boxId);
        const now = Date.now();
        const theme = {
            theme_id: `t_${now}`,
            rev: (box?.config?.theme?.rev || 0) + 1,
            theme_name: name,
            widgets,
            background,
            assets,
            updated_at: now,
            updated_by: uid,
        };
        await this.boxRepo.update(boxId, { 'config/theme': theme, updated_at: theme.updated_at });
        await this.boxRepo.updateFlags(boxId, { theme_flag: true });
        return theme;
    }
}
exports.ThemeService = ThemeService;
//# sourceMappingURL=theme.service.js.map