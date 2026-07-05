"use strict";
Object.defineProperty(exports, "__esModule", { value: true });
exports.FirebaseUserRepository = void 0;
const firebase_base_repository_1 = require("./firebase-base.repository");
class FirebaseUserRepository extends firebase_base_repository_1.FirebaseBaseRepository {
    constructor() {
        super('users');
    }
    // Add specific user repository methods here
    async linkBox(uid, boxId, role) {
        const ref = this.getRef(`${uid}/pairedBoxes/${boxId}`);
        await ref.set(role);
    }
    async unlinkBox(uid, boxId) {
        const ref = this.getRef(`${uid}/pairedBoxes/${boxId}`);
        await ref.remove();
    }
}
exports.FirebaseUserRepository = FirebaseUserRepository;
//# sourceMappingURL=firebase-user.repository.js.map