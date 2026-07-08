"use strict";
Object.defineProperty(exports, "__esModule", { value: true });
exports.AuthController = void 0;
const auth_service_1 = require("../services/auth.service");
class AuthController {
    constructor(authService = new auth_service_1.AuthService()) {
        this.authService = authService;
        // Not used directly if relying on Firebase Auth SDK on frontend, 
        // but useful if you want to sync users explicitly
        this.login = async (req, res, next) => {
            try {
                // In a real scenario, the token is verified in middleware, 
                // but here we might pass the token to the service for additional claims
                // assuming req.user is set by auth middleware
                res.status(200).json({ success: true, data: { message: 'Logged in' } });
            }
            catch (error) {
                next(error);
            }
        };
        this.deleteAccount = async (req, res, next) => {
            try {
                const uid = req.user.uid;
                const deletedBoxes = await this.authService.deleteAccount(uid);
                res.status(200).json({
                    success: true,
                    data: { deletedBoxes, message: 'Account permanently deleted' }
                });
            }
            catch (error) {
                next(error);
            }
        };
    }
}
exports.AuthController = AuthController;
//# sourceMappingURL=auth.controller.js.map