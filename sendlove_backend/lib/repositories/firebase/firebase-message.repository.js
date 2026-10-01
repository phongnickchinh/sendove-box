"use strict";
Object.defineProperty(exports, "__esModule", { value: true });
exports.FirebaseMessageRepository = void 0;
const firebase_1 = require("../../firebase");
class FirebaseMessageRepository {
    /** Create a message at messages/{boxId}/{messageId}. */
    async createMessage(boxId, messageId, data) {
        const ref = firebase_1.db.ref(`messages/${boxId}/${messageId}`);
        const record = { id: messageId, ...data };
        await ref.set(record);
        return record;
    }
    /** Get one message by id. */
    async getMessage(boxId, messageId) {
        const snapshot = await firebase_1.db.ref(`messages/${boxId}/${messageId}`).once('value');
        if (!snapshot.exists())
            return null;
        return snapshot.val();
    }
    /**
     * List the N newest messages, newest first.
     * The ESP32 compares the timestamp field with its local last_download_ts.
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
    /** Count messages within a time window (for rate limiting). */
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
    /** Soft-delete a message (sets deleted_at). */
    async softDelete(boxId, messageId) {
        await firebase_1.db.ref(`messages/${boxId}/${messageId}/deleted_at`).set(Date.now());
    }
}
exports.FirebaseMessageRepository = FirebaseMessageRepository;
//# sourceMappingURL=firebase-message.repository.js.map