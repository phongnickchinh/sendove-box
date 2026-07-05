"use strict";
Object.defineProperty(exports, "__esModule", { value: true });
exports.UserController = void 0;
const user_service_1 = require("../services/user.service");
class UserController {
    constructor() {
        this.getProfile = async (req, res, next) => {
            try {
                const uid = req.user.uid;
                const user = await this.userService.getUserProfile(uid);
                res.status(200).json({ success: true, data: user });
            }
            catch (error) {
                next(error);
            }
        };
        this.updateProfile = async (req, res, next) => {
            try {
                const uid = req.user.uid;
                const user = await this.userService.updateProfile(uid, req.body);
                res.status(200).json({ success: true, data: user });
            }
            catch (error) {
                next(error);
            }
        };
        this.userService = new user_service_1.UserService();
    }
}
exports.UserController = UserController;
//# sourceMappingURL=user.controller.js.map