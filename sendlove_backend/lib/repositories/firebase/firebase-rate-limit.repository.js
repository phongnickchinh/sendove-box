"use strict";
Object.defineProperty(exports, "__esModule", { value: true });
exports.FirebaseRateLimitRepository = void 0;
const firebase_1 = require("../../firebase");
class FirebaseRateLimitRepository {
    constructor() {
        this.basePath = 'rate_limits';
    }
    getKey(senderId, boxId) {
        return `${senderId}_${boxId}`;
    }
    /**
     * Lấy record rate limit cho cặp sender + box
     */
    async get(senderId, boxId) {
        const key = this.getKey(senderId, boxId);
        const snapshot = await firebase_1.db.ref(`${this.basePath}/${key}`).once('value');
        if (!snapshot.exists())
            return null;
        return snapshot.val();
    }
    /**
     * Tăng count lên 1
     */
    async increment(senderId, boxId) {
        const key = this.getKey(senderId, boxId);
        const ref = firebase_1.db.ref(`${this.basePath}/${key}/count`);
        await ref.transaction((current) => {
            return (current || 0) + 1;
        });
    }
    /**
     * Reset window mới: count = 1, window_start = now
     */
    async reset(senderId, boxId) {
        const key = this.getKey(senderId, boxId);
        await firebase_1.db.ref(`${this.basePath}/${key}`).set({
            count: 1,
            window_start: Date.now(),
        });
    }
    /**
     * Atomic check-and-increment sử dụng Firebase transaction.
     * Kết hợp đọc → kiểm tra → tăng count trong 1 bước duy nhất,
     * tránh race condition khi nhiều request đồng thời.
     *
     * Trả về { allowed: true } nếu dưới limit, { allowed: false, remainingMs } nếu bị giới hạn.
     */
    async checkAndIncrement(senderId, boxId, maxCount, windowMs) {
        const key = this.getKey(senderId, boxId);
        const ref = firebase_1.db.ref(`${this.basePath}/${key}`);
        const now = Date.now();
        const result = await ref.transaction((current) => {
            // Chưa có record → tạo mới, cho qua
            if (!current) {
                return { count: 1, window_start: now };
            }
            // Window hết hạn → reset, cho qua
            if (now - current.window_start > windowMs) {
                return { count: 1, window_start: now };
            }
            // Đã đạt giới hạn → abort transaction (return undefined)
            if (current.count >= maxCount) {
                return undefined;
            }
            // Dưới limit → tăng count
            return { count: current.count + 1, window_start: current.window_start };
        });
        if (!result.committed) {
            // Transaction bị abort = đã đạt rate limit
            const snapshot = await ref.once('value');
            const data = snapshot.val();
            const remainingMs = data ? windowMs - (now - data.window_start) : 0;
            return { allowed: false, remainingMs: Math.max(0, remainingMs) };
        }
        return { allowed: true };
    }
}
exports.FirebaseRateLimitRepository = FirebaseRateLimitRepository;
//# sourceMappingURL=firebase-rate-limit.repository.js.map