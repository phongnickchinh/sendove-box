"use strict";
Object.defineProperty(exports, "__esModule", { value: true });
const express_1 = require("express");
const user_controller_1 = require("../controllers/user.controller");
const auth_middleware_1 = require("../middleware/auth.middleware");
const router = (0, express_1.Router)();
const controller = new user_controller_1.UserController();
router.use(auth_middleware_1.requireAuth);
router.get('/me', controller.getProfile);
router.patch('/me', controller.updateProfile);
exports.default = router;
//# sourceMappingURL=user.routes.js.map