"use strict";
Object.defineProperty(exports, "__esModule", { value: true });
exports.requireProvisioningKey = void 0;
const error_handler_middleware_1 = require("./error-handler.middleware");
/**
 * Middleware xác thực thiết bị mới đăng ký bằng provisioning key.
 * ESP32 firmware phải gắn key này vào header 'X-Provisioning-Key' khi gọi /device/register.
 *
 * Key được cấu hình qua biến môi trường DEVICE_PROVISIONING_KEY.
 */
const requireProvisioningKey = (req, _res, next) => {
    const key = req.headers['x-provisioning-key'];
    const expectedKey = process.env.DEVICE_PROVISIONING_KEY;
    if (!expectedKey) {
        console.warn('[Provisioning] DEVICE_PROVISIONING_KEY is not set. Rejecting all device registrations.');
        return next(new error_handler_middleware_1.AppError(500, 'server_config_error', 'Device provisioning is not configured'));
    }
    if (!key || key !== expectedKey) {
        return next(new error_handler_middleware_1.AppError(401, 'unauthorized', 'Invalid or missing provisioning key'));
    }
    next();
};
exports.requireProvisioningKey = requireProvisioningKey;
//# sourceMappingURL=device-provisioning.middleware.js.map