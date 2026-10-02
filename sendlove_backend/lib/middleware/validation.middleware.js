"use strict";
Object.defineProperty(exports, "__esModule", { value: true });
exports.heartbeatSchema = exports.registerDeviceSchema = exports.updateAlarmSchema = exports.createAlarmSchema = exports.updateProfileSchema = exports.confirmMessageSchema = exports.initiateMessageSchema = exports.updateBoxConfigSchema = exports.updateWifiSchema = exports.pairBoxSchema = exports.validate = void 0;
const error_handler_middleware_1 = require("./error-handler.middleware");
/** Validation middleware factory. Whitelist: fields not in the schema are dropped from req.body. */
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
        // Replace body with the sanitized version — only whitelisted fields pass
        req.body = sanitized;
        next();
    };
};
exports.validate = validate;
// ---- Predefined validation schemas ----
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
/** PUT /boxes/:boxId/config */
exports.updateBoxConfigSchema = {
    led_state: { type: 'string', enum: ['OFF', 'BREATHING', 'SOLID', 'BLINK_FAST'] },
    display_brightness: { type: 'number', min: 0, max: 100 },
    playback_volume: { type: 'number', min: 0, max: 100 },
};
/** POST /boxes/:boxId/messages/initiate */
exports.initiateMessageSchema = {
    types: { type: 'array', required: true, itemType: 'string' },
};
/** POST /boxes/:boxId/messages/confirm */
exports.confirmMessageSchema = {
    message_id: { type: 'string', required: true },
    type: { type: 'string', required: true, enum: ['video', 'image', 'gif', 'voice', 'text'] },
    text: { type: 'string', maxLength: 500 },
    duration: { type: 'number', min: 0, max: 60 },
    frame_count: { type: 'number', min: 0, max: 1500 },
    width: { type: 'number', min: 1, max: 320 },
    height: { type: 'number', min: 1, max: 320 },
    uploaded_files: { type: 'array', itemType: 'string' },
};
/** PATCH /users/me — only display_name and avatar_url are editable */
exports.updateProfileSchema = {
    display_name: { type: 'string', minLength: 1, maxLength: 50 },
    avatar_url: { type: 'string', maxLength: 2048 },
};
/** A real 24h "HH:mm" (no "99:99", which the firmware would skip). Same regex as database.rules.json. */
const ALARM_TIME_PATTERN = /^([01]\d|2[0-3]):[0-5]\d$/;
/** POST /boxes/:boxId/alarms */
exports.createAlarmSchema = {
    time: { type: 'string', required: true, pattern: ALARM_TIME_PATTERN },
    is_enable: { type: 'boolean', required: true },
    repeatable: { type: 'boolean', required: true },
    // "" = no music (beep). < 24 chars: the firmware's musicId[24] buffer.
    music_id: { type: 'string', maxLength: 23 },
    volume: { type: 'number', min: 0, max: 100 },
    ramp: { type: 'boolean' },
};
/** PATCH /boxes/:boxId/alarms/:alarmId */
exports.updateAlarmSchema = {
    time: { type: 'string', pattern: ALARM_TIME_PATTERN },
    is_enable: { type: 'boolean' },
    repeatable: { type: 'boolean' },
    music_id: { type: 'string', maxLength: 23 },
    volume: { type: 'number', min: 0, max: 100 },
    ramp: { type: 'boolean' },
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
    storage_type: { type: 'string', enum: ['sd', 'nand'] },
};
//# sourceMappingURL=validation.middleware.js.map