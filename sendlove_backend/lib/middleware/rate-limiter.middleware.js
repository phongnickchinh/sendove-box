"use strict";
Object.defineProperty(exports, "__esModule", { value: true });
exports.messageSendRateLimit = void 0;
const error_handler_middleware_1 = require("./error-handler.middleware");
const firebase_rate_limit_repository_1 = require("../repositories/firebase/firebase-rate-limit.repository");
const config_1 = require("../config");
const rateLimitRepo = new firebase_rate_limit_repository_1.FirebaseRateLimitRepository();
/**
 * Rate Limiter Middleware cho việc gửi tin nhắn.
 * Giới hạn: N tin / 24 giờ cho mỗi cặp (sender_id + box_id).
 *
 * Logic:
 * 1. Đọc record rate_limits/{senderId}_{boxId}
 * 2. Nếu chưa có → tạo mới (count=1, window_start=now) → cho qua
 * 3. Nếu window hết hạn (now - window_start > 24h) → reset → cho qua
 * 4. Nếu count >= LIMIT → trả 429
 * 5. Nếu dưới limit → tăng count → cho qua
 */
const messageSendRateLimit = async (req, res, next) => {
    try {
        const senderId = req.user?.uid;
        const boxId = req.params.boxId;
        if (!senderId)
            throw new error_handler_middleware_1.AppError(401, 'unauthorized', 'User not authenticated');
        if (!boxId)
            throw new error_handler_middleware_1.AppError(400, 'bad_request', 'Missing boxId');
        const { maxMessagesPerWindow, windowDurationMs } = config_1.config.rateLimit;
        const now = Date.now();
        const record = await rateLimitRepo.get(senderId, boxId);
        if (!record) {
            // Lần gửi đầu tiên → tạo record mới
            await rateLimitRepo.reset(senderId, boxId);
            return next();
        }
        // Kiểm tra window đã hết hạn chưa
        if (now - record.window_start > windowDurationMs) {
            // Hết hạn → reset window mới
            await rateLimitRepo.reset(senderId, boxId);
            return next();
        }
        // Còn trong window → kiểm tra count
        if (record.count >= maxMessagesPerWindow) {
            const remainingMs = windowDurationMs - (now - record.window_start);
            const remainingHours = Math.ceil(remainingMs / (60 * 60 * 1000));
            throw new error_handler_middleware_1.AppError(429, 'rate_limit_exceeded', `Bạn đã gửi tối đa ${maxMessagesPerWindow} tin nhắn. Vui lòng thử lại sau ${remainingHours} giờ.`);
        }
        // Dưới limit → tăng count và cho qua
        await rateLimitRepo.increment(senderId, boxId);
        next();
    }
    catch (error) {
        next(error);
    }
};
exports.messageSendRateLimit = messageSendRateLimit;
//# sourceMappingURL=rate-limiter.middleware.js.map