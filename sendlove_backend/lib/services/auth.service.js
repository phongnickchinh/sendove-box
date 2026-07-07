"use strict";
Object.defineProperty(exports, "__esModule", { value: true });
exports.AuthService = void 0;
const firebase_user_repository_1 = require("../repositories/firebase/firebase-user.repository");
const error_handler_middleware_1 = require("../middleware/error-handler.middleware");
class AuthService {
    constructor() {
        this.userRepo = new firebase_user_repository_1.FirebaseUserRepository();
    }
    /**
     * Xử lý đăng nhập Google OAuth.
     * Tạo user mới nếu chưa tồn tại, cập nhật profile nếu đã có.
     */
    async handleGoogleLogin(decodedToken) {
        const { uid, email, name, picture } = decodedToken;
        const now = Date.now();
        let user = await this.userRepo.getById(uid);
        if (!user) {
            // Tạo user mới
            user = await this.userRepo.create(uid, {
                id: uid,
                email: email || '',
                display_name: name || '',
                is_admin: false,
                avatar_url: picture || null,
                last_login_at: now,
                boxes_list: {},
                created_at: now,
                updated_at: now,
            });
        }
        else {
            // Cập nhật profile nếu thay đổi + ghi last_login_at
            const updateData = { last_login_at: now, updated_at: now };
            if (name && user.display_name !== name)
                updateData.display_name = name;
            if (picture && user.avatar_url !== picture)
                updateData.avatar_url = picture;
            user = await this.userRepo.update(uid, updateData);
        }
        return user;
    }
    async deleteAccount(uid) {
        const user = await this.userRepo.getById(uid);
        if (!user)
            throw new error_handler_middleware_1.AppError(404, 'user_not_found', 'User not found');
        const deletedBoxes = [];
        // Thu thập danh sách box cần unpair
        if (user.boxes_list) {
            for (const boxId of Object.keys(user.boxes_list)) {
                deletedBoxes.push(boxId);
            }
        }
        // Xoá user khỏi RTDB
        await this.userRepo.delete(uid);
        return deletedBoxes;
    }
}
exports.AuthService = AuthService;
//# sourceMappingURL=auth.service.js.map