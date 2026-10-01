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
    /** Get the rate-limit record for a sender + box pair. */
    async get(senderId, boxId) {
        const key = this.getKey(senderId, boxId);
        const snapshot = await firebase_1.db.ref(`${this.basePath}/${key}`).once('value');
        if (!snapshot.exists())
            return null;
        return snapshot.val();
    }
    /** Increment count by 1. */
    async increment(senderId, boxId) {
        const key = this.getKey(senderId, boxId);
        const ref = firebase_1.db.ref(`${this.basePath}/${key}/count`);
        await ref.transaction((current) => {
            return (current || 0) + 1;
        });
    }
    /** Start a new window: count = 1, window_start = now. */
    async reset(senderId, boxId) {
        const key = this.getKey(senderId, boxId);
        await firebase_1.db.ref(`${this.basePath}/${key}`).set({
            count: 1,
            window_start: Date.now(),
        });
    }
    /**
     * Atomic check-and-increment in a Firebase transaction: read → check →
     * increment as one step, so concurrent requests can't race.
     *
     * Returns { allowed: true } under the limit, { allowed: false, remainingMs } when limited.
     */
    async checkAndIncrement(senderId, boxId, maxCount, windowMs) {
        const key = this.getKey(senderId, boxId);
        const ref = firebase_1.db.ref(`${this.basePath}/${key}`);
        const now = Date.now();
        const result = await ref.transaction((current) => {
            // No record → create it, allow
            if (!current) {
                return { count: 1, window_start: now };
            }
            // Window expired → reset, allow
            if (now - current.window_start > windowMs) {
                return { count: 1, window_start: now };
            }
            // Limit reached → abort the transaction (return undefined)
            if (current.count >= maxCount) {
                return undefined;
            }
            // Under the limit → increment
            return { count: current.count + 1, window_start: current.window_start };
        });
        if (!result.committed) {
            // Aborted transaction = rate limit reached
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