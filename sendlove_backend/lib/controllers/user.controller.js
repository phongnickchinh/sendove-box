"use strict";
Object.defineProperty(exports, "__esModule", { value: true });
exports.UserController = void 0;
const user_service_1 = require("../services/user.service");
class UserController {
    constructor(userService = new user_service_1.UserService()) {
        this.userService = userService;
        this.getProfile = async (req, res, next) => {
            try {
                const uid = req.user.uid;
                const email = req.user.email || '';
                const name = req.user?.name;
                const picture = req.user?.picture;
                const user = await this.userService.getOrCreateUserProfile(uid, email, name, picture);
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
    }
}
exports.UserController = UserController;
//# sourceMappingURL=user.controller.js.map