"use strict";
Object.defineProperty(exports, "__esModule", { value: true });
exports.FirebaseMessageRepository = void 0;
const firebase_1 = require("../../firebase");
class FirebaseMessageRepository {
    /**
     * Tạo message mới dưới node messages/{boxId}/{messageId}
     */
    async createMessage(boxId, messageId, data) {
        const ref = firebase_1.db.ref(`messages/${boxId}/${messageId}`);
        const record = { id: messageId, ...data };
        await ref.set(record);
        return record;
    }
    /**
     * Lấy 1 message theo ID
     */
    async getMessage(boxId, messageId) {
        const snapshot = await firebase_1.db.ref(`messages/${boxId}/${messageId}`).once('value');
        if (!snapshot.exists())
            return null;
        return snapshot.val();
    }
    /**
     * Liệt kê N message gần nhất, sắp xếp theo timestamp giảm dần.
     * ESP32 dùng trường timestamp để so sánh với last_download_ts nội bộ.
     */
    async listMessages(boxId, limit = 20) {
        const snapshot = await firebase_1.db.ref(`messages/${boxId}`)
            .orderByChild('timestamp')
            .limitToLast(limit)
            .once('value');
        if (!snapshot.exists())
            return [];
        const messagesObj = snapshot.val();
        const messages = [];
        for (const msgId in messagesObj) {
            messages.push(messagesObj[msgId]);
        }
        // Sort descending by timestamp
        return messages.sort((a, b) => b.timestamp - a.timestamp);
    }
    /**
     * Đếm số message trong 1 khoảng thời gian (dùng cho rate limiting)
     */
    async countMessagesSince(boxId, senderId, sinceTimestamp) {
        const snapshot = await firebase_1.db.ref(`messages/${boxId}`)
            .orderByChild('timestamp')
            .startAt(sinceTimestamp)
            .once('value');
        if (!snapshot.exists())
            return 0;
        const messagesObj = snapshot.val();
        let count = 0;
        for (const msgId in messagesObj) {
            if (messagesObj[msgId].sender_id === senderId) {
                count++;
            }
        }
        return count;
    }
    /**
     * Xoá mềm message (set deleted_at)
     */
    async softDelete(boxId, messageId) {
        await firebase_1.db.ref(`messages/${boxId}/${messageId}/deleted_at`).set(Date.now());
    }
}
exports.FirebaseMessageRepository = FirebaseMessageRepository;
//# sourceMappingURL=firebase-message.repository.js.map