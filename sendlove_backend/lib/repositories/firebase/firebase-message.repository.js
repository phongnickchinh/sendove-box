"use strict";
Object.defineProperty(exports, "__esModule", { value: true });
exports.FirebaseMessageRepository = void 0;
const firebase_1 = require("../../firebase");
class FirebaseMessageRepository {
    // Messages are stored under `messages/{boxId}/{messageId}`
    async createMessage(boxId, messageId, data) {
        const ref = firebase_1.db.ref(`messages/${boxId}/${messageId}`);
        await ref.set(data);
        return this.getMessage(boxId, messageId);
    }
    async getMessage(boxId, messageId) {
        const snapshot = await firebase_1.db.ref(`messages/${boxId}/${messageId}`).once('value');
        if (!snapshot.exists())
            return null;
        return { messageId, ...snapshot.val() };
    }
    async updateMessage(boxId, messageId, data) {
        const ref = firebase_1.db.ref(`messages/${boxId}/${messageId}`);
        await ref.update(data);
        return this.getMessage(boxId, messageId);
    }
    async listMessages(boxId, limit = 20) {
        const snapshot = await firebase_1.db.ref(`messages/${boxId}`)
            .orderByChild('createdAt')
            .limitToLast(limit)
            .once('value');
        if (!snapshot.exists())
            return [];
        const messagesObj = snapshot.val();
        const messages = [];
        for (const msgId in messagesObj) {
            messages.push({ messageId: msgId, ...messagesObj[msgId] });
        }
        // Sort descending by createdAt
        return messages.sort((a, b) => b.createdAt - a.createdAt);
    }
}
exports.FirebaseMessageRepository = FirebaseMessageRepository;
//# sourceMappingURL=firebase-message.repository.js.map