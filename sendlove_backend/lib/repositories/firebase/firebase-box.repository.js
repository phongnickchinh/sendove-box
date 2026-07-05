"use strict";
Object.defineProperty(exports, "__esModule", { value: true });
exports.FirebaseBoxRepository = void 0;
const firebase_base_repository_1 = require("./firebase-base.repository");
const firebase_1 = require("../../firebase");
class FirebaseBoxRepository extends firebase_base_repository_1.FirebaseBaseRepository {
    constructor() {
        super('boxes');
    }
    // Update polling cache for a box
    async updatePollingCache(boxId, data) {
        const ref = firebase_1.db.ref(`device_polling_cache/${boxId}`);
        await ref.update(data);
    }
    // Get polling cache for a box
    async getPollingCache(boxId) {
        const snapshot = await firebase_1.db.ref(`device_polling_cache/${boxId}`).once('value');
        if (!snapshot.exists())
            return null;
        return snapshot.val();
    }
    // Find box by pairing code (scode or rcode)
    async findByPairingCode(code) {
        const isSender = code.startsWith('SCODE');
        const field = isSender ? 'pairingInfo/senderCode' : 'pairingInfo/receiverCode';
        const snapshot = await firebase_1.db.ref(this.collectionPath)
            .orderByChild(field)
            .equalTo(code)
            .limitToFirst(1)
            .once('value');
        if (!snapshot.exists())
            return null;
        const boxes = snapshot.val();
        const boxId = Object.keys(boxes)[0];
        return { boxId, ...boxes[boxId] };
    }
}
exports.FirebaseBoxRepository = FirebaseBoxRepository;
//# sourceMappingURL=firebase-box.repository.js.map