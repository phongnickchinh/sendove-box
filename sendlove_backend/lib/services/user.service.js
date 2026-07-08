"use strict";
Object.defineProperty(exports, "__esModule", { value: true });
exports.UserService = void 0;
const firebase_user_repository_1 = require("../repositories/firebase/firebase-user.repository");
const error_handler_middleware_1 = require("../middleware/error-handler.middleware");
class UserService {
    constructor(userRepo = new firebase_user_repository_1.FirebaseUserRepository()) {
        this.userRepo = userRepo;
    }
    async getOrCreateUserProfile(uid, email, name, picture) {
        let user = await this.userRepo.getById(uid);
        if (!user) {
            const now = Date.now();
            const newUser = {
                id: uid,
                email: email,
                display_name: name || email.split('@')[0],
                avatar_url: picture || null,
                is_admin: false,
                last_login_at: now,
                created_at: now,
                updated_at: now,
                boxes_list: {}
            };
            user = await this.userRepo.create(uid, newUser);
        }
        else {
            await this.userRepo.updateLastLogin(uid);
        }
        return user;
    }
    async updateProfile(uid, data) {
        const user = await this.userRepo.getById(uid);
        if (!user)
            throw new error_handler_middleware_1.AppError(404, 'user_not_found', 'User not found');
        const updateData = { updated_at: Date.now() };
        if (data.display_name !== undefined)
            updateData.display_name = data.display_name;
        if (data.avatar_url !== undefined)
            updateData.avatar_url = data.avatar_url;
        return await this.userRepo.update(uid, updateData);
    }
}
exports.UserService = UserService;
//# sourceMappingURL=user.service.js.map