"use strict";
Object.defineProperty(exports, "__esModule", { value: true });
exports.FirebaseBoxRepository = void 0;
const firebase_base_repository_1 = require("./firebase-base.repository");
const firebase_1 = require("../../firebase");
class FirebaseBoxRepository extends firebase_base_repository_1.FirebaseBaseRepository {
    constructor() {
        super('boxes');
    }
    /**
     * Tìm box bằng mã pairing (scode hoặc rcode)
     */
    async findByPairingCode(code, codeType) {
        const field = `code/${codeType}`;
        const snapshot = await firebase_1.db.ref(this.collectionPath)
            .orderByChild(field)
            .equalTo(code)
            .limitToFirst(1)
            .once('value');
        if (!snapshot.exists())
            return null;
        const data = snapshot.val();
        const boxId = Object.keys(data)[0];
        return { id: boxId, ...data[boxId] };
    }
    /**
     * Cập nhật flags (a_flag, ota_flag, p_flag)
     */
    async updateFlags(boxId, flags) {
        await firebase_1.db.ref(`${this.collectionPath}/${boxId}/flags`).update(flags);
    }
    /**
     * Đọc flags hiện tại
     */
    async getFlags(boxId) {
        const snapshot = await firebase_1.db.ref(`${this.collectionPath}/${boxId}/flags`).once('value');
        if (!snapshot.exists())
            return null;
        return snapshot.val();
    }
    /**
     * Cập nhật status (online, battery, charging, last_seen, fw_version)
     */
    async updateStatus(boxId, status) {
        await firebase_1.db.ref(`${this.collectionPath}/${boxId}/status`).update(status);
    }
    /**
     * Đọc status hiện tại
     */
    async getStatus(boxId) {
        const snapshot = await firebase_1.db.ref(`${this.collectionPath}/${boxId}/status`).once('value');
        if (!snapshot.exists())
            return null;
        return snapshot.val();
    }
}
exports.FirebaseBoxRepository = FirebaseBoxRepository;
//# sourceMappingURL=firebase-box.repository.js.map