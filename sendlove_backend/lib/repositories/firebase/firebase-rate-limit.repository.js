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
}
exports.FirebaseRateLimitRepository = FirebaseRateLimitRepository;
//# sourceMappingURL=firebase-rate-limit.repository.js.map