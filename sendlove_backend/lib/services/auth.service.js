"use strict";
var __createBinding = (this && this.__createBinding) || (Object.create ? (function(o, m, k, k2) {
    if (k2 === undefined) k2 = k;
    var desc = Object.getOwnPropertyDescriptor(m, k);
    if (!desc || ("get" in desc ? !m.__esModule : desc.writable || desc.configurable)) {
      desc = { enumerable: true, get: function() { return m[k]; } };
    }
    Object.defineProperty(o, k2, desc);
}) : (function(o, m, k, k2) {
    if (k2 === undefined) k2 = k;
    o[k2] = m[k];
}));
var __setModuleDefault = (this && this.__setModuleDefault) || (Object.create ? (function(o, v) {
    Object.defineProperty(o, "default", { enumerable: true, value: v });
}) : function(o, v) {
    o["default"] = v;
});
var __importStar = (this && this.__importStar) || (function () {
    var ownKeys = function(o) {
        ownKeys = Object.getOwnPropertyNames || function (o) {
            var ar = [];
            for (var k in o) if (Object.prototype.hasOwnProperty.call(o, k)) ar[ar.length] = k;
            return ar;
        };
        return ownKeys(o);
    };
    return function (mod) {
        if (mod && mod.__esModule) return mod;
        var result = {};
        if (mod != null) for (var k = ownKeys(mod), i = 0; i < k.length; i++) if (k[i] !== "default") __createBinding(result, mod, k[i]);
        __setModuleDefault(result, mod);
        return result;
    };
})();
Object.defineProperty(exports, "__esModule", { value: true });
exports.AuthService = void 0;
const admin = __importStar(require("firebase-admin"));
const firebase_user_repository_1 = require("../repositories/firebase/firebase-user.repository");
const firebase_box_repository_1 = require("../repositories/firebase/firebase-box.repository");
const error_handler_middleware_1 = require("../middleware/error-handler.middleware");
const firebase_1 = require("../firebase");
class AuthService {
    constructor(userRepo = new firebase_user_repository_1.FirebaseUserRepository(), boxRepo = new firebase_box_repository_1.FirebaseBoxRepository()) {
        this.userRepo = userRepo;
        this.boxRepo = boxRepo;
    }
    /**
     * Handles Google OAuth sign-in: creates the user if missing, otherwise
     * updates the profile.
     */
    async handleGoogleLogin(decodedToken) {
        const { uid, email, name, picture } = decodedToken;
        const now = Date.now();
        let user = await this.userRepo.getById(uid);
        if (user && user.is_deleted) {
            throw new error_handler_middleware_1.AppError(403, 'account_deleted', 'Account has been deleted.');
        }
        if (!user) {
            // New user
            user = await this.userRepo.create(uid, {
                id: uid,
                email: email || '',
                display_name: name || '',
                is_admin: false,
                avatar_url: picture || null,
                is_deleted: false,
                last_login_at: now,
                boxes_list: {},
                created_at: now,
                updated_at: now,
            });
        }
        else {
            // Update the profile if it changed + record last_login_at
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
        // 1. Hard delete: unpair from every box (removes the denormalized copies)
        if (user.boxes_list) {
            for (const boxId of Object.keys(user.boxes_list)) {
                try {
                    const box = await this.boxRepo.getById(boxId);
                    if (box) {
                        const pairing = box.pairing || {};
                        const now = Date.now();
                        if (pairing.sender_id === uid) {
                            await this.boxRepo.update(boxId, {
                                'pairing/sender_id': null,
                                'pairing/sender_paired_time': null,
                                updated_at: now,
                            });
                        }
                        if (pairing.receiver_id === uid) {
                            await this.boxRepo.update(boxId, {
                                'pairing/receiver_id': null,
                                'pairing/receiver_paired_time': null,
                                updated_at: now,
                            });
                        }
                        // Tell the device its pairing changed
                        await this.boxRepo.updateFlags(boxId, { p_flag: true });
                    }
                }
                catch (error) {
                    console.warn(`[AuthService] Failed to unpair box ${boxId}:`, error);
                }
                deletedBoxes.push(boxId);
            }
        }
        // 2. Hard delete: this user's rate-limit records
        try {
            const rateLimitsSnapshot = await firebase_1.db.ref('rate_limits')
                .orderByKey()
                .startAt(`${uid}_`)
                .endAt(`${uid}_\uffff`)
                .once('value');
            if (rateLimitsSnapshot.exists()) {
                const updates = {};
                rateLimitsSnapshot.forEach((child) => {
                    updates[`rate_limits/${child.key}`] = null;
                    return false;
                });
                await firebase_1.db.ref().update(updates);
            }
        }
        catch (error) {
            console.warn(`[AuthService] Failed to clean rate limits for ${uid}:`, error);
        }
        // 3. Soft delete: mark the user deleted (data kept for audit/history)
        await this.userRepo.softDelete(uid);
        // 4. Hard delete: the Firebase Auth user (prevents signing in again)
        try {
            await admin.auth().deleteUser(uid);
        }
        catch (error) {
            console.warn(`[AuthService] Failed to delete Firebase Auth user ${uid}:`, error);
        }
        return deletedBoxes;
    }
}
exports.AuthService = AuthService;
//# sourceMappingURL=auth.service.js.map