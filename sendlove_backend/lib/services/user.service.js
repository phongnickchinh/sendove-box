"use strict";
Object.defineProperty(exports, "__esModule", { value: true });
exports.UserService = void 0;
const firebase_user_repository_1 = require("../repositories/firebase/firebase-user.repository");
const error_handler_middleware_1 = require("../middleware/error-handler.middleware");
class UserService {
    constructor() {
        this.userRepo = new firebase_user_repository_1.FirebaseUserRepository();
    }
    async getUserProfile(uid) {
        const user = await this.userRepo.getById(uid);
        if (!user)
            throw new error_handler_middleware_1.AppError(404, 'user_not_found', 'User not found');
        return user;
    }
    async updateProfile(uid, data) {
        const user = await this.userRepo.getById(uid);
        if (!user)
            throw new error_handler_middleware_1.AppError(404, 'user_not_found', 'User not found');
        const updateData = {};
        if (data.displayName !== undefined)
            updateData.displayName = data.displayName;
        if (data.photoURL !== undefined)
            updateData.photoURL = data.photoURL;
        return await this.userRepo.update(uid, updateData);
    }
}
exports.UserService = UserService;
//# sourceMappingURL=user.service.js.map