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
     * Handle user login via Google OAuth token validation.
     * In a real implementation, you might exchange a custom token or just ensure the user exists in RTDB.
     */
    async handleGoogleLogin(decodedToken) {
        const { uid, email, name, picture } = decodedToken;
        let user = await this.userRepo.getById(uid);
        if (!user) {
            // Create new user
            user = await this.userRepo.create(uid, {
                uid,
                email: email || null,
                displayName: name || null,
                photoURL: picture || null,
                createdAt: Date.now(),
            });
        }
        else {
            // Update profile info if changed
            let changed = false;
            if (name && user.displayName !== name) {
                user.displayName = name;
                changed = true;
            }
            if (picture && user.photoURL !== picture) {
                user.photoURL = picture;
                changed = true;
            }
            if (changed) {
                user = await this.userRepo.update(uid, {
                    displayName: user.displayName,
                    photoURL: user.photoURL
                });
            }
        }
        return user;
    }
    async deleteAccount(uid) {
        const user = await this.userRepo.getById(uid);
        if (!user)
            throw new error_handler_middleware_1.AppError(404, 'user_not_found', 'User not found');
        const deletedBoxes = [];
        // Unpair from all boxes
        if (user.pairedBoxes) {
            for (const [boxId] of Object.entries(user.pairedBoxes)) {
                // We just return the list of boxes here, 
                // the BoxService should ideally handle the actual unpairing logic on the Box node
                deletedBoxes.push(boxId);
            }
        }
        // Delete user from RTDB
        await this.userRepo.delete(uid);
        return deletedBoxes;
    }
}
exports.AuthService = AuthService;
//# sourceMappingURL=auth.service.js.map