"use strict";
Object.defineProperty(exports, "__esModule", { value: true });
exports.default = authRoutes;
const express_1 = require("express");
const auth_middleware_1 = require("../middleware/auth.middleware");
function authRoutes(controller) {
    const router = (0, express_1.Router)();
    // POST /auth/google is mostly handled by Frontend + Firebase SDK, but we can expose it if needed
    router.post('/google', auth_middleware_1.requireAuth, controller.login);
    router.delete('/account', auth_middleware_1.requireAuth, controller.deleteAccount);
    return router;
}
//# sourceMappingURL=auth.routes.js.map