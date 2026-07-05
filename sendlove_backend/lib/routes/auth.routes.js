"use strict";
Object.defineProperty(exports, "__esModule", { value: true });
const express_1 = require("express");
const auth_controller_1 = require("../controllers/auth.controller");
const auth_middleware_1 = require("../middleware/auth.middleware");
const router = (0, express_1.Router)();
const controller = new auth_controller_1.AuthController();
// POST /auth/google is mostly handled by Frontend + Firebase SDK, but we can expose it if needed
router.post('/google', auth_middleware_1.requireAuth, controller.login);
router.delete('/account', auth_middleware_1.requireAuth, controller.deleteAccount);
exports.default = router;
//# sourceMappingURL=auth.routes.js.map