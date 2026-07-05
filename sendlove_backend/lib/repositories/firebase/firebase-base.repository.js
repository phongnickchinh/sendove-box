"use strict";
Object.defineProperty(exports, "__esModule", { value: true });
exports.FirebaseBaseRepository = void 0;
const firebase_1 = require("../../firebase");
class FirebaseBaseRepository {
    constructor(collectionPath) {
        this.collectionPath = collectionPath;
    }
    getRef(id) {
        return firebase_1.db.ref(`${this.collectionPath}/${id}`);
    }
    async create(id, data) {
        const ref = this.getRef(id);
        await ref.set(data);
        return this.getById(id);
    }
    async getById(id) {
        const snapshot = await this.getRef(id).once('value');
        if (!snapshot.exists()) {
            return null;
        }
        return snapshot.val();
    }
    async update(id, data) {
        const ref = this.getRef(id);
        await ref.update(data);
        return this.getById(id);
    }
    async delete(id) {
        await this.getRef(id).remove();
    }
}
exports.FirebaseBaseRepository = FirebaseBaseRepository;
//# sourceMappingURL=firebase-base.repository.js.map