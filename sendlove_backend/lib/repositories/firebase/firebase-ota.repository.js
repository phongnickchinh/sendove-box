"use strict";
Object.defineProperty(exports, "__esModule", { value: true });
exports.FirebaseOtaRepository = void 0;
const firebase_base_repository_1 = require("./firebase-base.repository");
const firebase_1 = require("../../firebase");
class FirebaseOtaRepository extends firebase_base_repository_1.FirebaseBaseRepository {
    constructor() {
        super('ota_tasks');
    }
    /**
     * Cập nhật trạng thái OTA task
     */
    async updateOtaStatus(taskId, status, extras) {
        const updateData = {
            status,
            updated_at: Date.now(),
            ...extras,
        };
        await firebase_1.db.ref(`${this.collectionPath}/${taskId}`).update(updateData);
    }
    /**
     * Tìm OTA task đang pending cho box cụ thể
     */
    async findPendingByBoxId(boxId) {
        const snapshot = await firebase_1.db.ref(this.collectionPath)
            .orderByChild('box_id')
            .equalTo(boxId)
            .once('value');
        if (!snapshot.exists())
            return null;
        const tasks = snapshot.val();
        for (const taskId in tasks) {
            if (tasks[taskId].status === 'pending') {
                return { id: taskId, ...tasks[taskId] };
            }
        }
        return null;
    }
}
exports.FirebaseOtaRepository = FirebaseOtaRepository;
//# sourceMappingURL=firebase-ota.repository.js.map