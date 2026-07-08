"use strict";
Object.defineProperty(exports, "__esModule", { value: true });
exports.heartbeatSchema = exports.registerDeviceSchema = exports.updateAlarmSchema = exports.createAlarmSchema = exports.updateProfileSchema = exports.confirmMessageSchema = exports.initiateMessageSchema = exports.updateWifiSchema = exports.pairBoxSchema = exports.validate = void 0;
const error_handler_middleware_1 = require("./error-handler.middleware");
/**
 * Validation middleware factory.
 * Chỉ các field có trong schema mới được giữ lại trong req.body (whitelist).
 * Các field không khai báo sẽ bị lọc bỏ → chống mass-assignment.
 */
const validate = (schema) => {
    return (req, _res, next) => {
        const errors = [];
        const sanitized = {};
        for (const [field, rule] of Object.entries(schema)) {
            const value = req.body[field];
            // Required check
            if (rule.required && (value === undefined || value === null || value === '')) {
                errors.push(`'${field}' is required`);
                continue;
            }
            // Skip optional missing fields
            if (value === undefined || value === null)
                continue;
            // Type check
            if (rule.type === 'array') {
                if (!Array.isArray(value)) {
                    errors.push(`'${field}' must be an array`);
                    continue;
                }
                if (rule.itemType && !value.every((item) => typeof item === rule.itemType)) {
                    errors.push(`'${field}' items must be of type ${rule.itemType}`);
                    continue;
                }
            }
            else if (typeof value !== rule.type) {
                errors.push(`'${field}' must be of type ${rule.type}`);
                continue;
            }
            // String validations
            if (rule.type === 'string' && typeof value === 'string') {
                if (rule.minLength !== undefined && value.trim().length < rule.minLength) {
                    errors.push(`'${field}' must be at least ${rule.minLength} characters`);
                    continue;
                }
                if (rule.maxLength !== undefined && value.length > rule.maxLength) {
                    errors.push(`'${field}' must be at most ${rule.maxLength} characters`);
                    continue;
                }
                if (rule.pattern && !rule.pattern.test(value)) {
                    errors.push(`'${field}' has an invalid format`);
                    continue;
                }
            }
            // Number validations
            if (rule.type === 'number' && typeof value === 'number') {
                if (rule.min !== undefined && value < rule.min) {
                    errors.push(`'${field}' must be >= ${rule.min}`);
                    continue;
                }
                if (rule.max !== undefined && value > rule.max) {
                    errors.push(`'${field}' must be <= ${rule.max}`);
                    continue;
                }
            }
            // Enum check
            if (rule.enum && !rule.enum.includes(value)) {
                errors.push(`'${field}' must be one of: ${rule.enum.join(', ')}`);
                continue;
            }
            sanitized[field] = value;
        }
        if (errors.length > 0) {
            return next(new error_handler_middleware_1.AppError(400, 'validation_error', errors.join('; ')));
        }
        // Replace body with sanitized version — chỉ whitelisted fields đi qua
        req.body = sanitized;
        next();
    };
};
exports.validate = validate;
// ==========================================
// Predefined Validation Schemas
// ==========================================
/** POST /boxes/pair */
exports.pairBoxSchema = {
    pairingCode: { type: 'string', required: true, pattern: /^[SR][A-Z0-9]{6,9}$/ },
    boxName: { type: 'string', required: true, minLength: 1, maxLength: 50 },
};
/** PUT /boxes/:boxId/wifi */
exports.updateWifiSchema = {
    ssid: { type: 'string', required: true, minLength: 1, maxLength: 32 },
    password: { type: 'string', maxLength: 63 },
};
/** POST /boxes/:boxId/messages/initiate */
exports.initiateMessageSchema = {
    types: { type: 'array', required: true, itemType: 'string' },
};
/** POST /boxes/:boxId/messages/confirm */
exports.confirmMessageSchema = {
    message_id: { type: 'string', required: true },
    text: { type: 'string', maxLength: 500 },
    bin_url: { type: 'string', maxLength: 2048 },
    voice_url: { type: 'string', maxLength: 2048 },
    gif_url: { type: 'string', maxLength: 2048 },
    bg_music_url: { type: 'string', maxLength: 2048 },
    image_url: { type: 'string', maxLength: 2048 },
    total_size: { type: 'number', min: 0 },
    thumbnail_url: { type: 'string', maxLength: 2048 },
};
/** PATCH /users/me — chỉ cho phép sửa display_name và avatar_url */
exports.updateProfileSchema = {
    display_name: { type: 'string', minLength: 1, maxLength: 50 },
    avatar_url: { type: 'string', maxLength: 2048 },
};
/** POST /boxes/:boxId/alarms */
exports.createAlarmSchema = {
    time: { type: 'string', required: true, pattern: /^\d{2}:\d{2}$/ },
    is_enable: { type: 'boolean', required: true },
    repeatable: { type: 'boolean', required: true },
};
/** PATCH /boxes/:boxId/alarms/:alarmId */
exports.updateAlarmSchema = {
    time: { type: 'string', pattern: /^\d{2}:\d{2}$/ },
    is_enable: { type: 'boolean' },
    repeatable: { type: 'boolean' },
};
/** POST /device/register */
exports.registerDeviceSchema = {
    deviceId: { type: 'string', required: true, minLength: 1, maxLength: 64 },
    mac_address: { type: 'string', maxLength: 17 },
    fw_version: { type: 'string', required: true, minLength: 1, maxLength: 20 },
};
/** POST /device/heartbeat */
exports.heartbeatSchema = {
    battery: { type: 'number', min: 0, max: 100 },
    charging: { type: 'boolean' },
    fw_version: { type: 'string', maxLength: 20 },
};
//# sourceMappingURL=validation.middleware.js.map