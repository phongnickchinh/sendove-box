"use strict";
Object.defineProperty(exports, "__esModule", { value: true });
exports.FirebaseUserRepository = void 0;
const firebase_base_repository_1 = require("./firebase-base.repository");
class FirebaseUserRepository extends firebase_base_repository_1.FirebaseBaseRepository {
    constructor() {
        super('users');
    }
    /**
     * Liên kết user với box (ghi vào boxes_list)
     */
    async linkBox(uid, boxId, entry) {
        const ref = this.getRef(`${uid}/boxes_list/${boxId}`);
        await ref.set(entry);
    }
    /**
     * Ngắt liên kết user với box
     */
    async unlinkBox(uid, boxId) {
        const ref = this.getRef(`${uid}/boxes_list/${boxId}`);
        await ref.remove();
    }
    /**
     * Cập nhật last_login_at
     */
    async updateLastLogin(uid) {
        await this.getRef(`${uid}/last_login_at`).set(Date.now());
    }
}
exports.FirebaseUserRepository = FirebaseUserRepository;
//# sourceMappingURL=firebase-user.repository.js.map