"use strict";
Object.defineProperty(exports, "__esModule", { value: true });
exports.messageSendRateLimit = void 0;
const error_handler_middleware_1 = require("./error-handler.middleware");
const firebase_rate_limit_repository_1 = require("../repositories/firebase/firebase-rate-limit.repository");
const config_1 = require("../config");
const rateLimitRepo = new firebase_rate_limit_repository_1.FirebaseRateLimitRepository();
/**
 * Rate limiter for sending messages: N messages / 24 hours per
 * (sender_id + box_id) pair.
 *
 * 1. Read rate_limits/{senderId}_{boxId}
 * 2. Missing → create (count=1, window_start=now) → allow
 * 3. Window expired (now - window_start > 24h) → reset → allow
 * 4. count >= LIMIT → 429
 * 5. Under the limit → increment → allow
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
        // Atomic: check + increment in a single transaction → no race condition
        const result = await rateLimitRepo.checkAndIncrement(senderId, boxId, maxMessagesPerWindow, windowDurationMs);
        if (!result.allowed) {
            const remainingHours = Math.ceil((result.remainingMs || 0) / (60 * 60 * 1000));
            throw new error_handler_middleware_1.AppError(429, 'rate_limit_exceeded', `Bạn đã gửi tối đa ${maxMessagesPerWindow} tin nhắn. Vui lòng thử lại sau ${remainingHours} giờ.`);
        }
        next();
    }
    catch (error) {
        next(error);
    }
};
exports.messageSendRateLimit = messageSendRateLimit;
//# sourceMappingURL=rate-limiter.middleware.js.map