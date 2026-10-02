"use strict";
Object.defineProperty(exports, "__esModule", { value: true });
exports.ThemeService = exports.DATE_FORMATS = exports.FONT_MAX_BYTES = exports.BG_BYTES = exports.SCREEN = void 0;
exports.sanitizeWidgets = sanitizeWidgets;
const firebase_box_repository_1 = require("../repositories/firebase/firebase-box.repository");
const firebase_storage_repository_1 = require("../repositories/firebase/firebase-storage.repository");
const error_handler_middleware_1 = require("../middleware/error-handler.middleware");
const crc32_1 = require("../utils/crc32");
exports.SCREEN = 240;
/** 240 × 240 × 2 bytes of RGB565 — the background size LayoutEngine reads from the theme partition. */
exports.BG_BYTES = exports.SCREEN * exports.SCREEN * 2;
/** VLW fonts are subset by the web to the needed characters: a few dozen glyphs, a few dozen KB at most. */
exports.FONT_MAX_BYTES = 64 * 1024;
const MAX_WIDGETS = 8;
/** LayoutEngine::formatDate — the web subsets fonts from exactly these strings. */
exports.DATE_FORMATS = ['WD, DD.MM', 'WD DD.MM', 'DD/MM/YYYY'];
const LOCALES = ['vi', 'en'];
/**
 * Per-type rules: only the keys the firmware reads for that type. Fonts:
 * 'f_time'/'f_date' = VLW files in the package (must be present in `fonts`),
 * 'Font7'/'Font2' = firmware built-ins. Any other name is rejected: the firmware
 * would silently fall back to a built-in font.
 */
const RULES = {
    clock_time: { color: true, align: true, fonts: ['f_time', 'Font7'] },
    clock_date: { color: true, align: true, fonts: ['f_date', 'Font2'], date: true },
    chip_temp: { color: true, align: true },
    wifi_icon: { color: true, align: false },
    battery_icon: { color: false, align: false }, // multi-color pushImage, ignores cfg.color
};
const HEX = /^#[0-9A-Fa-f]{6}$/;
const isInt = (v) => typeof v === 'number' && Number.isInteger(v);
/** Validate + normalize widgets; throws a 400 with a readable reason. */
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
        // LayoutEngine.cpp's strcmp(type, ...) has no null check — a widget without a type crashes the box.
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
            // Without a color the firmware draws BLACK (default "#000000"), not white — store it explicitly.
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
            // Defaults to the built-in font (the last element): needs no font file.
            const font = raw.font ?? rule.fonts[rule.fonts.length - 1];
            if (!rule.fonts.includes(font)) {
                throw new error_handler_middleware_1.AppError(400, 'invalid_theme', `${where}.font must be one of: ${rule.fonts.join(', ')}`);
            }
            out.font = font;
            // Only so the web can reopen the editor as saved (the family + size subset into the VLW file).
            // The firmware ignores both keys: the glyphs are already in the font file.
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
    /** The saved theme (null = never saved), with a 15-minute signed URL so the web can preview the background. */
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
    /** Issue a signed POST policy so the web uploads the background straight to Storage. */
    async initiateBackgroundUpload(boxId) {
        const path = `${this.bgPrefix(boxId)}bg_${Date.now()}.bin`;
        const upload = await this.storageRepo.generateUploadPolicy(path, 'application/octet-stream', exports.BG_BYTES, 15);
        return { path, upload };
    }
    /** Same as the background, for a VLW font file (subset by the web). */
    async initiateFontUpload(boxId) {
        const path = `${this.bgPrefix(boxId)}f_${Date.now()}_${Math.floor(Math.random() * 1000)}.vlw`;
        const upload = await this.storageRepo.generateUploadPolicy(path, 'application/octet-stream', exports.FONT_MAX_BYTES, 15);
        return { path, upload };
    }
    /**
     * Validate one package file (inside THIS box's theme folder, right extension),
     * then measure size + crc32 from the actual file.
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
        // VLW fonts: a widget using 'f_time'/'f_date' MUST come with the matching file, or the
        // box draws with a built-in font while the web previews another -> looks like a bug.
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