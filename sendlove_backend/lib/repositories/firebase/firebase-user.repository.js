"use strict";
Object.defineProperty(exports, "__esModule", { value: true });
exports.FirebaseUserRepository = void 0;
const firebase_base_repository_1 = require("./firebase-base.repository");
class FirebaseUserRepository extends firebase_base_repository_1.FirebaseBaseRepository {
    constructor() {
        super('users');
    }
    /** Link a user to a box (writes boxes_list). */
    async linkBox(uid, boxId, entry) {
        const ref = this.getRef(`${uid}/boxes_list/${boxId}`);
        await ref.set(entry);
    }
    /** Unlink a user from a box. */
    async unlinkBox(uid, boxId) {
        const ref = this.getRef(`${uid}/boxes_list/${boxId}`);
        await ref.remove();
    }
    /** Update last_login_at. */
    async updateLastLogin(uid) {
        await this.getRef(`${uid}/last_login_at`).set(Date.now());
    }
    /**
     * Soft-delete a user: sets is_deleted = true and deleted_at.
     * The user's data is kept for audit/history.
     */
    async softDelete(uid) {
        const now = Date.now();
        await this.getRef(uid).update({
            is_deleted: true,
            deleted_at: now,
            updated_at: now,
        });
    }
}
exports.FirebaseUserRepository = FirebaseUserRepository;
//# sourceMappingURL=firebase-user.repository.js.map